// ============================================================
// PhysicsEngine.cpp — Jolt Physics 集成实现
//
// 【职责】把 ECS 实体与 Jolt 物理世界双向同步
//
// 【每帧数据流】PhysicsSystem::update()
//   ①a 清理：已删除实体的 Jolt Body 销毁（防幽灵碰撞体）
//   ①b 创建/重建：新实体或碰撞盒尺寸变化 → 建 Jolt Body
//   ②  模拟：固定步长 1/60s（多余时间累加到下帧）
//   ③  回写：Dynamic 刹体位置 → TransformComponent（物理驱动渲染）
//
// 【碰撞事件 → 脚本】
//   Jolt 工作线程检测到碰撞 → EcsContactListener::OnContactAdded
//   → 推 DeferredSignal 到队列（线程安全）
//   → 主线程 ScriptSystem 消费队列 → 脚本里 connect("collision", cb) 收到
//
// 【Jolt 两层碰撞系统】（为了性能）
//   ObjectLayer（精）: NON_MOVING(静态) / MOVING(动态)
//   BroadPhaseLayer（粗）: 对应的包围盒树，静态物体不用每帧更新
//   规则：静态只跟动态撞，动态跟所有撞（两个静态物体不检测）
//
// 【pImpl】Jolt 头文件只在本 .cpp，PhysicsSystem.h 不依赖 Jolt
// ============================================================

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>            // RegisterTypes（注册碰撞处理器）
#include <Jolt/Core/Factory.h>             // Factory（反序列化工厂，必需）
#include <Jolt/Core/TempAllocator.h>       // 模拟临时内存池
#include <Jolt/Core/JobSystemThreadPool.h> // Jolt 自己的多线程任务系统
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h> // 盒形碰撞体
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>        // 创建/删除/查询刹体
#include <Jolt/Physics/Collision/ContactListener.h> // 碰撞回调
#include <Jolt/Physics/Collision/RayCast.h>         // 射线查询（玩法射线：子弹/视线）
#include <Jolt/Physics/Body/BodyFilter.h>           // IgnoreSingleBodyFilter（射线忽略自己）
#include <Jolt/Physics/Collision/CastResult.h>

#include "Systems/PhysicsSystem.h"
#include "Components/TransformComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Core/SignalQueue.h" // 碰撞信号走 Core 层信号队列（不再依赖脚本层，ADR-002）

#include <thread>
#include <map>
#include <algorithm> // std::max/std::min（线程数钳制、dt 死亡螺旋保护）

JPH_SUPPRESS_WARNINGS

// ---- 碰撞层定义（对标 HelloWorld 官方示例）----
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
}

/// 对象层碰撞规则：静态只跟动态撞，动态跟所有撞
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
    {
        switch (a)
        {
        case Layers::NON_MOVING:
            return b == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};

/// 对象层 → 宽相层映射
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        m_map[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_map[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override { return m_map[l]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "L"; }
#endif
private:
    JPH::BroadPhaseLayer m_map[Layers::NUM_LAYERS];
};

/// 对象层 vs 宽相层碰撞规则
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override
    {
        switch (a)
        {
        case Layers::NON_MOVING:
            return b == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};

/// 碰撞监听器：Jolt 工作线程回调 → 推信号队列 → 主线程调 Lua 回调
/// （复用现有的 DeferredSignal 机制，天然线程安全）
class EcsContactListener : public JPH::ContactListener
{
public:
    void OnContactAdded(const JPH::Body &body1, const JPH::Body &body2,
                        const JPH::ContactManifold &, JPH::ContactSettings &) override
    {
        // 创建 Body 时把实体 ID 存在了 UserData 里
        auto e1 = (flecs::entity_t)body1.GetUserData();
        auto e2 = (flecs::entity_t)body2.GetUserData();
        if (!e1 || !e2)
            return;

        // 触发器（sensor）接触 → "trigger_enter"（Godot Area3D 的 body_entered）；
        // 普通碰撞 → "collision"。双向发：两个实体的脚本都能 connect 收到
        const char *sig = (body1.IsSensor() || body2.IsSensor())
                              ? "trigger_enter"
                              : "collision";
        pushDeferredSignal({e1, sig, {(double)e2}});
        pushDeferredSignal({e2, sig, {(double)e1}});
    }

    /// 接触分离（触发器"离开区域"/普通碰撞分开都发）。
    /// 此回调只有 BodyID 拿不到 UserData → 用 body 创建时登记的映射表反查
    void OnContactRemoved(const JPH::SubShapeIDPair &pair) override
    {
        flecs::entity_t e1 = 0, e2 = 0;
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            auto i1 = bodyToEntity.find(pair.GetBody1ID().GetIndexAndSequenceNumber());
            auto i2 = bodyToEntity.find(pair.GetBody2ID().GetIndexAndSequenceNumber());
            if (i1 == bodyToEntity.end() || i2 == bodyToEntity.end())
                return;
            e1 = i1->second;
            e2 = i2->second;
        }
        pushDeferredSignal({e1, "collision_end", {(double)e2}});
        pushDeferredSignal({e2, "collision_end", {(double)e1}});
    }

    // BodyID → 实体（OnContactRemoved 反查用；创建/销毁 body 时维护）
    std::map<uint32_t, flecs::entity_t> bodyToEntity;
    std::mutex mapMutex; // Jolt 工作线程读、主线程写
};

// ============================================================
// PhysicsSystem::Impl — Jolt 世界（pImpl）
// ============================================================
struct PhysicsSystem::Impl
{
    JPH::TempAllocatorImpl *tempAllocator = nullptr;
    JPH::JobSystemThreadPool *jobSystem = nullptr;
    JPH::PhysicsSystem physicsSystem;

    BPLayerInterfaceImpl bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objVsBpFilter;
    ObjectLayerPairFilterImpl objPairFilter;
    EcsContactListener contactListener;

    std::map<flecs::entity_t, JPH::BodyID> bodies;    // 实体 → Jolt Body
    std::map<flecs::entity_t, glm::vec3> bodyExtents; // 创建时的尺寸（检测变化用）
    float accumulator = 0;                            // 固定步长累加器

    // ★ 物理插值（Godot 同款）：物理 60Hz、渲染帧率任意，两者节拍不合时
    //   直接用"最新物理位置"渲染会看到 60Hz 的阶梯抖动。
    //   保存每个 Dynamic 刚体最近两次物理步的位置（prev/curr），
    //   渲染回写时按 accumulator/kStep 在两者间 lerp → 任意帧率都平滑。
    std::map<flecs::entity_t, glm::vec3> prevPos; // 上一物理步位置
    std::map<flecs::entity_t, glm::vec3> currPos; // 当前物理步位置
    bool snapshotValid = false;                   // 首帧/重置后无快照，直接用最新位置
};

/// 全局 Impl 指针：供脚本物理 API（physicsApplyImpulse 等）访问
static PhysicsSystem::Impl *g_physicsImpl = nullptr;

void PhysicsSystem::init()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_impl = new Impl;
    m_impl->tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    // 工作线程数：hardware_concurrency() 标准允许返回 0（unsigned），
    // 直接 -1 会下溢成 42 亿；返回 1 时得 0 线程。钳到至少 1（审查 P7）
    const int hwThreads = (int)std::thread::hardware_concurrency();
    const int physicsWorkers = std::max(1, hwThreads - 1);
    m_impl->jobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, physicsWorkers);

    // 初始化物理世界（容量按小游戏配置）
    m_impl->physicsSystem.Init(
        1024, // maxBodies
        0,    // numBodyMutexes (0=默认)
        1024, // maxBodyPairs
        1024, // maxContactConstraints
        m_impl->bpLayerInterface,
        m_impl->objVsBpFilter,
        m_impl->objPairFilter);

    // 重力
    m_impl->physicsSystem.SetGravity(JPH::Vec3(0, -9.81f, 0));

    // 碰撞监听（Jolt 工作线程回调 → 信号队列 → 主线程 Lua）
    m_impl->physicsSystem.SetContactListener(&m_impl->contactListener);

    g_physicsImpl = m_impl; // 脚本物理 API 的入口
}

void PhysicsSystem::shutdown()
{
    if (!m_impl)
        return;
    g_physicsImpl = nullptr;
    delete m_impl->jobSystem;
    delete m_impl->tempAllocator;
    delete m_impl;
    m_impl = nullptr;

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::resetBodies()
{
    if (!m_impl)
        return;
    auto &bodyIf = m_impl->physicsSystem.GetBodyInterface();
    for (auto &[eid, bodyId] : m_impl->bodies)
    {
        bodyIf.RemoveBody(bodyId);
        bodyIf.DestroyBody(bodyId);
    }
    m_impl->bodies.clear();
    m_impl->bodyExtents.clear();
    m_impl->accumulator = 0;
    // 插值快照一并清空（下次 Play 从新位置起步，不残留旧位置）
    m_impl->prevPos.clear();
    m_impl->currPos.clear();
    m_impl->snapshotValid = false;
    {
        std::lock_guard<std::mutex> lk(m_impl->contactListener.mapMutex);
        m_impl->contactListener.bodyToEntity.clear();
    }
}

void PhysicsSystem::update(flecs::world &world, float dt)
{
    if (!m_impl)
        return;

    auto &bodyIf = m_impl->physicsSystem.GetBodyInterface();

    // ①a 清理"该销毁的刚体"：实体已死 / 或实体还活着但 Rigidbody/BoxCollider
    //   组件被移除（编辑器删组件）——后者不清会留下看不见但撞得到的幽灵体（P6）。
    //   bodyExtents 和插值快照必须一起 erase，否则随实体销毁缓慢泄漏（P12）。
    for (auto it = m_impl->bodies.begin(); it != m_impl->bodies.end();)
    {
        flecs::entity e = world.entity(it->first);
        bool gone = !e.is_alive() ||
                    !e.has<RigidbodyComponent>() || !e.has<BoxColliderComponent>();
        if (gone)
        {
            bodyIf.RemoveBody(it->second);
            bodyIf.DestroyBody(it->second);
            flecs::entity_t id = it->first;
            uint32_t bodyNum = it->second.GetIndexAndSequenceNumber();
            it = m_impl->bodies.erase(it);
            m_impl->bodyExtents.erase(id); // P12：随体销毁，不再只增不减
            m_impl->prevPos.erase(id);     // 插值快照一并清
            m_impl->currPos.erase(id);
            {
                std::lock_guard<std::mutex> lk(m_impl->contactListener.mapMutex);
                m_impl->contactListener.bodyToEntity.erase(bodyNum);
            }
        }
        else
            ++it;
    }

    // ①b 发现新实体/尺寸变化 → 创建（或重建）Jolt Body
    world.each([&](flecs::entity e, TransformComponent &tf,
                   RigidbodyComponent &rb, BoxColliderComponent &box)
               {
        glm::vec3 extents(box.halfExtents.x * tf.scale.x,
                          box.halfExtents.y * tf.scale.y,
                          box.halfExtents.z * tf.scale.z);

        auto found = m_impl->bodies.find(e.raw_id());
        if (found != m_impl->bodies.end())
        {
            // 已存在：检查碰撞盒尺寸是否被编辑器改过 → 变了就重建
            auto &old = m_impl->bodyExtents[e.raw_id()];
            if (glm::length(old - extents) < 0.0001f) return; // 没变，跳过
            bodyIf.RemoveBody(found->second);
            bodyIf.DestroyBody(found->second);
            m_impl->bodies.erase(found);
        }

        JPH::EMotionType motion = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer layer = Layers::MOVING;
        if (rb.motionType == 0) { motion = JPH::EMotionType::Static;    layer = Layers::NON_MOVING; }
        if (rb.motionType == 1) { motion = JPH::EMotionType::Kinematic; layer = Layers::MOVING; }

        JPH::BoxShapeSettings shapeSettings(JPH::Vec3(extents.x, extents.y, extents.z));
        auto shapeResult = shapeSettings.Create();
        if (shapeResult.HasError()) return;

        JPH::BodyCreationSettings settings(
            shapeResult.Get(),
            JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
            JPH::Quat::sIdentity(),
            motion, layer);
        settings.mFriction = rb.friction;
        settings.mRestitution = rb.restitution;
        settings.mUserData = e.raw_id(); // 碰撞监听器通过它找回实体
        // 触发器（Godot Area3D）：不阻挡只发信号（trigger_enter/collision_end）
        settings.mIsSensor = box.isTrigger;
        // ★ 应用组件质量：不设的话 Jolt 按体积×密度(1000kg/m³)自动算——
        //   1x1x1 方块=1000kg！apply_impulse(6) 只推出 0.006m/s，
        //   跳跃看起来"完全失效"（实为被千倍质量吞掉，实猎的 bug）
        if (motion == JPH::EMotionType::Dynamic)
        {
            settings.mOverrideMassProperties =
                JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rb.mass > 0 ? rb.mass : 1.0f;
        }

        JPH::BodyID id = bodyIf.CreateAndAddBody(settings, JPH::EActivation::Activate);
        m_impl->bodies[e.raw_id()] = id;
        m_impl->bodyExtents[e.raw_id()] = extents;
        rb.bodyId = id.GetIndexAndSequenceNumber();
        {
            // 登记 BodyID→实体（OnContactRemoved 反查用，Jolt 工作线程会读）
            std::lock_guard<std::mutex> lk(m_impl->contactListener.mapMutex);
            m_impl->contactListener.bodyToEntity[rb.bodyId] = e.raw_id();
        } });

    // ② 固定步长模拟（Jolt 推荐 1/60）+ 插值快照采样
    constexpr float kStep = 1.0f / 60.0f;
    // ★ 死亡螺旋保护：断点/卡顿后某帧 dt 可能是几秒，不钳制会连跑几百步
    //   物理→越跑越卡的恶性循环。上限 0.25s（最多追赶 15 步），与
    //   Time.h 的 rawDt 钳制同阈值（审查 P5：那套保护之前没接到物理）。
    if (dt > 0.25f)
        dt = 0.25f;
    m_impl->accumulator += dt;
    bool stepped = false;
    while (m_impl->accumulator >= kStep)
    {
        // 步进前：当前快照降级为"上一步"（只在真的要步进时才滚动）
        m_impl->prevPos = m_impl->currPos;
        m_impl->physicsSystem.Update(kStep, 1, m_impl->tempAllocator, m_impl->jobSystem);
        m_impl->accumulator -= kStep;
        stepped = true;
    }
    if (stepped)
    {
        // 步进后：采样所有 Dynamic 刚体的最新位置为"当前快照"
        m_impl->currPos.clear();
        for (auto &[eid, bodyId] : m_impl->bodies)
        {
            JPH::RVec3 jp = bodyIf.GetCenterOfMassPosition(bodyId);
            m_impl->currPos[eid] = {(float)jp.GetX(), (float)jp.GetY(), (float)jp.GetZ()};
        }
        // 首次步进 prev 为空 → 用 curr 补齐（避免从原点滑入）
        if (!m_impl->snapshotValid)
        {
            m_impl->prevPos = m_impl->currPos;
            m_impl->snapshotValid = true;
        }
    }

    // ③ Dynamic 刚体位置写回 Transform（物理驱动渲染 + 插值平滑）
    //
    // 插值：渲染帧通常落在两次物理步之间（accumulator ∈ [0,kStep)），
    //   直接用最新位置会看到 60Hz 阶梯抖动；用 prev→curr 按 alpha 插值，
    //   任意渲染帧率都平滑（代价：画面比物理晚最多 1 个物理步 ≈16ms，无感）。
    //
    // 关键：Jolt 返回的是世界坐标，但 tf.position 是【局部】坐标（相对父）。
    //   — 无父实体：世界=局部，直接写
    //   — 有父实体：把世界坐标用父世界矩阵的逆变换回局部空间
    //     （否则 TransformSystem 会再乘一次父变换 → 位置翻倍）
    const float alpha = glm::clamp(m_impl->accumulator / kStep, 0.0f, 1.0f);
    world.each([&](flecs::entity e, TransformComponent &tf, RigidbodyComponent &rb)
               {
        auto it = m_impl->bodies.find(e.raw_id());
        if (it == m_impl->bodies.end()) return;
        if (rb.motionType != 2) return; // 只有 Dynamic 需要回写

        // 优先用插值快照；无快照（刚创建/刚重置）回退最新物理位置
        glm::vec3 worldPos;
        auto itCurr = m_impl->currPos.find(e.raw_id());
        auto itPrev = m_impl->prevPos.find(e.raw_id());
        if (itCurr != m_impl->currPos.end() && itPrev != m_impl->prevPos.end())
        {
            worldPos = glm::mix(itPrev->second, itCurr->second, alpha);
        }
        else
        {
            JPH::RVec3 jp = bodyIf.GetCenterOfMassPosition(it->second);
            worldPos = {(float)jp.GetX(), (float)jp.GetY(), (float)jp.GetZ()};
        }

        flecs::entity parent = e.parent();
        if (parent && parent.has<TransformComponent>())
        {
            // 世界坐标 → 父局部坐标（用父上一帧的 worldMatrix 逆矩阵）
            const auto *pt = static_cast<const TransformComponent *>(ecs_get_id(
                e.world().c_ptr(), parent.raw_id(),
                e.world().component<TransformComponent>().raw_id()));
            glm::vec4 local = glm::inverse(pt->worldMatrix) * glm::vec4(worldPos, 1.0f);
            tf.position = glm::vec3(local);
        }
        else
        {
            tf.position = worldPos;
        } });
}

// ============================================================
// 脚本物理 API 实现（LuaApi.cpp 调用）
// ============================================================

bool physicsApplyImpulse(flecs::entity_t id, float x, float y, float z)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    auto &bi = g_physicsImpl->physicsSystem.GetBodyInterface();
    bi.ActivateBody(it->second); // 防睡眠（同 physicsSetVelocity）
    bi.AddImpulse(it->second, JPH::Vec3(x, y, z));
    return true;
}

bool physicsSetVelocity(flecs::entity_t id, float x, float y, float z)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    auto &bi = g_physicsImpl->physicsSystem.GetBodyInterface();
    // ★ 先唤醒：Jolt 刚体静止后自动睡眠，SetLinearVelocity 不会唤醒
    //   睡眠刚体 → 玩家落地站一会儿后 WASD 失效（推不动，实踩坑）
    bi.ActivateBody(it->second);
    bi.SetLinearVelocity(it->second, JPH::Vec3(x, y, z));
    return true;
}

// 读取刚体线速度（玩家移动需要：水平速度改、竖直速度保留——
// 否则 set_velocity 会把跳跃/重力的 y 速度抹掉）
bool physicsGetVelocity(flecs::entity_t id, float &x, float &y, float &z)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    JPH::Vec3 v = g_physicsImpl->physicsSystem.GetBodyInterface().GetLinearVelocity(it->second);
    x = v.GetX();
    y = v.GetY();
    z = v.GetZ();
    return true;
}

// ============================================================
// 角色控制器（对标 Godot CharacterBody3D 的两大件）
// 之前每个玩家脚本手写"get_velocity 保 y + raycast 判落地"20 行样板，
// 现在引擎内建：move_slide 两行走人，is_on_floor 一行判跳。
// ============================================================

// 水平移动（自动保留竖直速度）：重力/跳跃的 y 分量不被抹掉
bool physicsMoveSlide(flecs::entity_t id, float vx, float vz)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    auto &bi = g_physicsImpl->physicsSystem.GetBodyInterface();
    float vy = bi.GetLinearVelocity(it->second).GetY(); // 保留竖直分量
    bi.SetLinearVelocity(it->second, JPH::Vec3(vx, vy, vz));
    bi.ActivateBody(it->second); // 睡眠的刚体推不动（实踩坑）
    return true;
}

// 落地判定：从刚体底面向下短射线（忽略自己）。
// 半高从创建时登记的 bodyExtents 拿——不用调用者操心碰撞体尺寸
bool physicsIsOnFloor(flecs::entity_t id)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    JPH::RVec3 pos = g_physicsImpl->physicsSystem.GetBodyInterface()
                         .GetPosition(it->second);
    float halfY = 0.5f;
    auto ext = g_physicsImpl->bodyExtents.find(id);
    if (ext != g_physicsImpl->bodyExtents.end())
        halfY = ext->second.y;
    // 从中心向下：半高 + 0.5 容差（地面在脚下 >0.5m 以下，
    // 旧 0.15 容差打不到地面导致 is_on_floor 永久 false→跳不了）
    flecs::entity_t hitE = 0;
    float hx, hy, hz, hd;
    return physicsRaycast((float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ(),
                          0, -1, 0, halfY + 0.5f, hitE, hx, hy, hz, hd, id);
}

// 传送刚体到指定位置（读档/重生/传送门必需）
// ★ 只改 Transform 没用：PhysicsSystem 每帧用刚体位置回写 Transform，
//   脚本 set_position 一帧后被拉回原地（读档瞬移失效的实 bug）——
//   位置的"真身"在物理引擎里，必须连刚体一起搬
bool physicsSetPosition(flecs::entity_t id, float x, float y, float z)
{
    if (!g_physicsImpl)
        return false;
    auto it = g_physicsImpl->bodies.find(id);
    if (it == g_physicsImpl->bodies.end())
        return false;
    g_physicsImpl->physicsSystem.GetBodyInterface().SetPosition(
        it->second, JPH::RVec3(x, y, z), JPH::EActivation::Activate);
    // 传送必须同步重置插值快照：否则渲染会从旧位置"滑"到新位置（拖影）
    g_physicsImpl->prevPos[id] = {x, y, z};
    g_physicsImpl->currPos[id] = {x, y, z};
    return true;
}

// ============================================================
// physicsRaycast — 玩法射线（子弹命中/敌人视线/地面检测）
//
// 【与编辑器拾取（Core/Raycast.h）的分工】
//   编辑器拾取：打得中所有可见网格（含无碰撞体的装饰物），自写数学
//   玩法射线：只打得中物理碰撞体，走 Jolt（BVH 加速、工业级精度）
//   Godot 同样分法：编辑器点选 vs PhysicsDirectSpaceState.intersect_ray
//
// 命中实体回映射：建 Body 时 mUserData 存了实体 ID（与碰撞监听同源）
// ============================================================
bool physicsRaycast(float ox, float oy, float oz,
                    float dx, float dy, float dz, float maxDist,
                    flecs::entity_t &hitEntity,
                    float &hitX, float &hitY, float &hitZ, float &hitDist,
                    flecs::entity_t ignoreEntity)
{
    if (!g_physicsImpl || maxDist <= 0)
        return false;

    // 方向归一化（调用方可能传非单位向量）
    JPH::Vec3 dir(dx, dy, dz);
    if (dir.LengthSq() < 1e-12f)
        return false;
    dir = dir.Normalized();

    // Jolt 的 RayCast：起点 + "带长度的方向向量"（非单位）
    JPH::RRayCast ray{JPH::RVec3(ox, oy, oz), dir * maxDist};
    JPH::RayCastResult hit; // mFraction ∈ [0,1]

    // ★ 忽略发射者自己：不忽略的话，从自身中心/内部发射的射线
    //   会以 fraction=0 命中自己 → 落地检测永远"命中"或起点微调地狱
    JPH::BodyID ignoreBody;
    if (ignoreEntity)
    {
        auto it = g_physicsImpl->bodies.find(ignoreEntity);
        if (it != g_physicsImpl->bodies.end())
            ignoreBody = it->second;
    }
    JPH::IgnoreSingleBodyFilter bodyFilter(ignoreBody); // 无效 BodyID = 不忽略任何

    if (!g_physicsImpl->physicsSystem.GetNarrowPhaseQuery().CastRay(
            ray, hit,
            JPH::BroadPhaseLayerFilter{}, // 默认：所有层都测
            JPH::ObjectLayerFilter{},
            bodyFilter))
        return false;

    // BodyID → 实体 ID（创建时存在 mUserData）
    hitEntity = (flecs::entity_t)g_physicsImpl->physicsSystem
                    .GetBodyInterface()
                    .GetUserData(hit.mBodyID);

    JPH::RVec3 p = ray.GetPointOnRay(hit.mFraction);
    hitX = (float)p.GetX();
    hitY = (float)p.GetY();
    hitZ = (float)p.GetZ();
    hitDist = hit.mFraction * maxDist;
    return true;
}
