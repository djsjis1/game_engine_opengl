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
#include "Scripting/LuaApi.h" // DeferredSignal 队列（碰撞信号推送）

#include <thread>
#include <map>

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

        // 双向发碰撞信号：两个实体都能在脚本里 connect("collision", cb) 收到
        std::lock_guard<std::mutex> lock(g_signalMutex);
        g_signalQueue.push({e1, "collision", {(double)e2}});
        g_signalQueue.push({e2, "collision", {(double)e1}});
    }
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
    m_impl->jobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() - 1);

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
}

void PhysicsSystem::update(flecs::world &world, float dt)
{
    if (!m_impl)
        return;

    auto &bodyIf = m_impl->physicsSystem.GetBodyInterface();

    // ①a 清理已删除实体的刹体（防幽灵碰撞体）
    for (auto it = m_impl->bodies.begin(); it != m_impl->bodies.end();)
    {
        if (!world.entity(it->first).is_alive())
        {
            bodyIf.RemoveBody(it->second);
            bodyIf.DestroyBody(it->second);
            it = m_impl->bodies.erase(it);
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
        rb.bodyId = id.GetIndexAndSequenceNumber(); });

    // ② 固定步长模拟（Jolt 推荐 1/60）
    constexpr float kStep = 1.0f / 60.0f;
    m_impl->accumulator += dt;
    while (m_impl->accumulator >= kStep)
    {
        m_impl->physicsSystem.Update(kStep, 1, m_impl->tempAllocator, m_impl->jobSystem);
        m_impl->accumulator -= kStep;
    }

    // ③ Dynamic 刹体位置写回 Transform（物理驱动渲染）
    //
    // 关键：Jolt 返回的是世界坐标，但 tf.position 是【局部】坐标（相对父）。
    //   — 无父实体：世界=局部，直接写
    //   — 有父实体：把世界坐标用父世界矩阵的逆变换回局部空间
    //     （否则 TransformSystem 会再乘一次父变换 → 位置翻倍）
    world.each([&](flecs::entity e, TransformComponent &tf, RigidbodyComponent &rb)
               {
        auto it = m_impl->bodies.find(e.raw_id());
        if (it == m_impl->bodies.end()) return;
        if (rb.motionType != 2) return; // 只有 Dynamic 需要回写

        JPH::RVec3 jp = bodyIf.GetCenterOfMassPosition(it->second);
        glm::vec3 worldPos((float)jp.GetX(), (float)jp.GetY(), (float)jp.GetZ());

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
