// ============================================================
// TweenSystem.cpp — 属性补间实现
//
// 每帧：elapsed += dt → t = ease(easing, elapsed/duration)
//       → 值 = mix(from, to, t) 写回 Transform → 完成回收+发信号
// 启动时捕获 from = 当前值（所以"从哪开始"永远自然衔接）。
// ============================================================
#include "Systems/TweenSystem.h"
#include "Components/TransformComponent.h"
#include "Systems/PhysicsSystem.h"   // position 补间同步物理体（防回写拉扯）
#include "Core/SignalQueue.h"        // 完成信号走 Core 层信号队列（不再依赖脚本层，ADR-002）
#include "Core/Easing.h"
#include <glm/glm.hpp>
#include <vector>

namespace
{
    enum class Target : int
    {
        kPosition = 0,
        kRotation = 1,
        kScale = 2,
    };

    struct ActiveTween
    {
        flecs::entity_t entity = 0;
        Target target = Target::kPosition;
        glm::vec3 from{0.0f}, to{0.0f};
        float duration = 0.5f;
        float elapsed = 0.0f;
        int easing = 0;
        bool fromCaptured = false; // 首帧才读当前值（启动时组件可能还没建好）
        std::string doneSignal;    // 非空：完成时给实体发此信号
    };

    std::vector<ActiveTween> g_tweens;   // 活动补间（主线程独占，无需锁）
    std::vector<ActiveTween> g_incoming; // 本帧新投递（update 开头合并）
} // namespace

void tweenStart(flecs::entity_t id, const std::string &target,
                float x, float y, float z, float duration,
                int easing, const std::string &doneSignal)
{
    ActiveTween t;
    t.entity = id;
    if (target == "rotation")
        t.target = Target::kRotation;
    else if (target == "scale")
        t.target = Target::kScale;
    else
        t.target = Target::kPosition;
    t.to = {x, y, z};
    t.duration = duration > 0.001f ? duration : 0.001f;
    t.easing = easing;
    t.doneSignal = doneSignal;
    g_incoming.push_back(std::move(t));
}

void TweenSystem::clearAll()
{
    g_tweens.clear();
    g_incoming.clear();
}

void TweenSystem::update(flecs::world &world, float dt)
{
    if (!isEnabled())
        return;

    // 合并新投递：同实体同属性的旧补间被顶掉（Godot 后发优先）
    for (auto &in : g_incoming)
    {
        for (auto it = g_tweens.begin(); it != g_tweens.end();)
        {
            if (it->entity == in.entity && it->target == in.target)
                it = g_tweens.erase(it);
            else
                ++it;
        }
        g_tweens.push_back(std::move(in));
    }
    g_incoming.clear();

    if (g_tweens.empty())
        return;

    auto tfCompId = world.component<TransformComponent>().raw_id();

    for (auto it = g_tweens.begin(); it != g_tweens.end();)
    {
        ActiveTween &tw = *it;
        flecs::entity e = world.entity(tw.entity);
        auto *tf = e.is_alive()
                       ? (TransformComponent *)ecs_get_mut_id(world.c_ptr(), tw.entity, tfCompId)
                       : nullptr;
        if (!tf)
        {
            it = g_tweens.erase(it); // 实体没了：补间作废
            continue;
        }

        glm::vec3 *field = tw.target == Target::kPosition   ? &tf->position
                           : tw.target == Target::kRotation ? &tf->rotation
                                                            : &tf->scale;
        if (!tw.fromCaptured)
        {
            tw.from = *field; // 首帧捕获起点：从当前值自然衔接
            tw.fromCaptured = true;
        }

        tw.elapsed += dt;
        float raw = tw.elapsed / tw.duration;
        bool done = raw >= 1.0f;
        float t = ease((EasingType)tw.easing, done ? 1.0f : raw);
        *field = tw.from + (tw.to - tw.from) * t;

        // position 补间同步物理体：带刚体的门/平台不被物理回写拉回
        if (tw.target == Target::kPosition)
            physicsSetPosition(tw.entity, field->x, field->y, field->z);

        if (done)
        {
            if (!tw.doneSignal.empty())
                // 完成信号走延迟队列（下帧初分发）——补间完成时正在遍历
                // g_tweens，直接调 Lua 回调若又发起新 tween 会改容器（自踩）
                pushDeferredSignal({tw.entity, tw.doneSignal, {}});
            it = g_tweens.erase(it);
        }
        else
            ++it;
    }
}
