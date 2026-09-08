#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include <algorithm>
#include <memory>
#include <map>

#ifndef Q_MOC_RUN
#pragma push_macro("emit")
#undef emit
#pragma push_macro("signals")
#undef signals
#include <flecs.h>
#pragma pop_macro("signals")
#pragma pop_macro("emit")
#endif

#ifndef Q_MOC_RUN
#include "Core/EngineFSM.h" // HFSM2 状态机（模板重，MOC 不碰）
#endif
#include "Core/System.h"
#include "Core/EngineState.h"   // 位掩码（供 System::activeIn() 使用）
#include "Core/SceneSnapshot.h" // 完整场景快照（Play/Stop 隔离）
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/TagComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/CameraComponent.h"
#include "Components/EditorVisible.h"
#include "Components/ScriptComponent.h"
#include "Components/SignalComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/LightComponent.h"
#include "Components/EnvironmentComponent.h"
#include "Components/AudioComponent.h"       // OnRemove observer 释放音源
#include "Components/ParticleComponent.h"    // 粒子发射器（Inspector 可调）
#include "Systems/AudioSystem.h"             // audioReleaseHandle（接口，零 miniaudio 依赖）
#include "Render/ResourceManager.h"          // Mesh OnRemove Observer 自动释放资源
#include "Generated/ComponentReflection.inl" // ★ reflect_gen 自动生成的反射注册

// 前置声明（实现在 Scripting/LuaApi.cpp）：实体销毁时清理信号连接的 Lua
// 注册表引用。用前置声明而非 include LuaApi.h——避免给 Engine.h 的众多
// 下游文件拉入 lua 头（解耦：Engine 不该依赖脚本层的完整头）。
void cleanupEntitySignals(flecs::world world, flecs::entity_t deadId);

// ============================================================
// Engine — HFSM2 状态机 + ECS Pipeline
//
// 状态机（HFSM2）：
//   setState(Play) → m_fsm.immediateChangeTo<PlayState>()
//   → PlayState::enter() → m_ctx.onEnterPlay() → UI 回调
//   → m_fsm.update() 处理过渡
//
// 管线：
//   runFrame(dt) → m_fsm.update() → 按 phase 排序 → 遍历启用 System
//
// 加新状态只需：
//   1. EngineState.h 加枚举值
//   2. EngineFSM.h 加状态类 + 回调
//   3. 各 System 的 activeIn() 加对应位
// ============================================================
class Engine
{
    void registerReflection()
    {
        // 基础类型：glm::vec3 的字段结构（组件反射的依赖，手写保留）
        // ⚠ 教训（MODULES.md 坑8）：必须三参数 member<T>(名, 1, 偏移)！
        // 两参数形式 flecs 会用字段间隙推断数组 count，推错 → 序列化数据错乱
        m_world.component<glm::vec3>()
            .member<float>("x", 1, offsetof(glm::vec3, x))
            .member<float>("y", 1, offsetof(glm::vec3, y))
            .member<float>("z", 1, offsetof(glm::vec3, z));

        // ★ 组件字段注册已自动化（mini-UHT）：
        // reflect_gen 扫描 Components/*.h 的 // @reflect 标记生成下面这个函数。
        // 加新组件/新字段：只需在头文件字段行尾加 // @reflect，重新构建即可，
        // 不再手写 member/offsetof（杜绝写错偏移/漏注册）
        registerGeneratedReflection(m_world);

        m_world.component<TransformComponent>().add<EditorVisible>();
        m_world.component<MeshComponent>().add<EditorVisible>();
        m_world.component<MaterialComponent>().add<EditorVisible>();
        m_world.component<RotateBehavior>().add<EditorVisible>();
        m_world.component<CameraComponent>().add<EditorVisible>();
        m_world.component<ScriptComponent>().add<EditorVisible>();
        m_world.component<RigidbodyComponent>().add<EditorVisible>();
        m_world.component<BoxColliderComponent>().add<EditorVisible>();
        // AnimationComponent：float/bool 字段走自动反射，char[](animFile/current/prev)手工补充
        m_world.component<AnimationComponent>().add<EditorVisible>();
        m_world.component<HealthComponent>().add<EditorVisible>();
        // SpriteComponent：frame 走自动反射，folder(char[]) 特殊面板+手工序列化
        m_world.component<SpriteComponent>().add<EditorVisible>();
        // 粒子发射器：全 REFLECT，Inspector 调参即预览
        m_world.component<ParticleComponent>().add<EditorVisible>();
    }

public:
    Engine()
    {
        // 注册组件类型（让 flecs 知道它们的存在）。
        // 注：下面大部分组件在 registerReflection() 里会被再次调用（幂等，不重复注册），
        // 这里保留是为了确保“不进 Inspector”的组件（Tag/Signal）也被注册。
        m_world.component<EditorVisible>();
        m_world.component<TransformComponent>();
        m_world.component<MeshComponent>();
        m_world.component<MaterialComponent>();
        m_world.component<TagComponent>();
        m_world.component<RotateBehavior>();
        m_world.component<CameraComponent>();
        m_world.component<ScriptComponent>();
        m_world.component<SignalComponent>();
        m_world.component<RigidbodyComponent>();
        m_world.component<BoxColliderComponent>();
        m_world.component<AnimationComponent>();
        m_world.component<HealthComponent>();
        m_world.component<SpriteComponent>();
        registerReflection(); // 注册组件反射

        // ★ 资源自动释放（解耦关键）：MeshComponent 被移除时自动归还引用。
        // 覆盖所有死亡路径：destruct/级联删除/手动移除组件 ——
        // 任何调用方都不再需要"记得释放"（releaseMesh 是纯记账，无 GL 依赖）
        m_world.observer<MeshComponent>()
            .event(flecs::OnRemove)
            .each([](flecs::entity, MeshComponent &m)
                  { ResourceManager::instance().releaseMesh(m.meshHandle); });

        // 精灵帧集同模式自动释放
        m_world.observer<SpriteComponent>()
            .event(flecs::OnRemove)
            .each([](flecs::entity, SpriteComponent &s)
                  { ResourceManager::instance().releaseSprite(s.spriteHandle); });

        // 音源同模式自动释放（P4：挂循环音效的实体销毁后，声音必须停+销毁，
        // 否则永远播下去——听得见的 bug）。audioReleaseHandle 内部判空安全。
        m_world.observer<AudioComponent>()
            .event(flecs::OnRemove)
            .each([](flecs::entity, AudioComponent &a)
                  { audioReleaseHandle(a.soundHandle); });

        // 信号连接自动清理（P1：防 Lua 注册表泄漏 + 孤儿回调）。
        // OnRemove 时 SignalComponent 仍可读，cleanupEntitySignals 遍历
        // signalMap/listensTo 逐个 unref。无脚本 VM 时内部安全跳过。
        m_world.observer<SignalComponent>()
            .event(flecs::OnRemove)
            .each([](flecs::entity e, SignalComponent &)
                  { cleanupEntitySignals(e.world(), e.raw_id()); });
    }

    ~Engine()
    {
        for (auto &s : m_systems)
            s->shutdown();
        m_systems.clear();
    }

    // ---- HFSM2 状态机 ----

    /// 当前状态（位掩码，供 System 过滤 + MainWindow 查询）
    EngineState state() const
    {
        if (m_fsm.isActive<PlayState>())
            return EngineState::Play;
        return EngineState::Edit;
    }

    /// 切换状态（按钮点击 / Tab 键触发）
    void setPlay() { m_fsm.immediateChangeTo<PlayState>(); }
    void setEdit() { m_fsm.immediateChangeTo<EditState>(); }

    /// 获取 Context 引用，供 MainWindow 注册 enter/exit 回调
    EngineContext &context() { return m_ctx; }

    /// HFSM2 状态 enter/exit 时调用的公开入口（全部委托给一个私有方法）
    void onEnterEdit() { applyStateFilter(); }
    void onExitEdit() { applyStateFilter(); }
    void onEnterPlay() { applyStateFilter(); }
    void onExitPlay() { applyStateFilter(); }

    // ---- 场景快照（对标 Godot：Play 时完整复制，Stop 后完美还原）----

    /// 进入 Play 前调用：完整序列化场景到内存（全组件，不只是 Transform）
    void snapshotScene() { m_snapshot.capture(m_world); }

    /// 退出 Play 后调用：清空世界 + 从内存快照完整重建
    void restoreScene() { m_snapshot.restore(m_world); }

    // ---- System 管理 ----

    void addSystem(std::unique_ptr<System> sys)
    {
        // 新系统立即按当前状态设置启用（不依赖下一次状态切换才生效）
        sys->setEnabled(sys->activeIn() & (int)state());
        m_systems.push_back(std::move(sys));
        std::sort(m_systems.begin(), m_systems.end(),
                  [](const auto &a, const auto &b)
                  { return a->phase() < b->phase(); });
    }

    // ---- 主循环 ----

    void runFrame(float dt)
    {
        m_fsm.update();

        for (auto &s : m_systems)
            if (s->isEnabled())
                s->update(m_world, dt);
    }

    flecs::world &world() { return m_world; }

private:
    flecs::world m_world;                           // ECS 世界
    std::vector<std::unique_ptr<System>> m_systems; // 系统列表

    EngineContext m_ctx; // 引擎上下文
    // ⚠ 声明顺序不可交换：m_fsm 持有 m_ctx 的引用（ContextT<EngineContext&>），
    //   m_ctx 必须先于 m_fsm 构造、晚于 m_fsm 析构（成员按声明序构造/逆序析构）
    FSM::Instance m_fsm{m_ctx};

    SceneSnapshot m_snapshot; // 完整场景快照（Play/Stop 隔离，对标 Godot）

    /// 统一入口：遍历所有 System，按当前状态启用/禁用
    void applyStateFilter()
    {
        int mask = (int)state();
        for (auto &s : m_systems)
            s->setEnabled(s->activeIn() & mask);
    }
};

#endif
