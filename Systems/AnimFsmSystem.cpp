// ============================================================
// AnimFsmSystem.cpp — 动画状态机实现
//
// 每帧：对每个状态机 → 找当前状态的出边 → 条件成立 → 切换：
//   写 AnimationComponent 的 crossfade 字段（prev=旧 current=新 blend）
//   —— 与 anim_crossfade API 完全同一套混合管线（AnimationSystem 采样）。
// ============================================================
#include "Systems/AnimFsmSystem.h"
#include "Components/AnimationComponent.h"
#include <cstring>
#include <map>
#include <vector>

namespace
{
    struct Transition
    {
        std::string from, to; // 状态名 = clip 名
        std::string param;    // 判定参数
        bool greater = true;  // true: param > threshold；false: <
        float threshold = 0.0f;
        float blend = 0.2f; // crossfade 时长
    };

    struct Machine
    {
        std::string state;                   // 当前状态
        std::vector<Transition> transitions; // 转换规则（声明期填好）
        std::map<std::string, float> params; // 脚本每帧喂的参数
        bool started = false;                // start 前不判定
    };

    std::map<flecs::entity_t, Machine> g_machines; // 主线程独占
} // namespace

void animFsmAddTransition(flecs::entity_t id, const std::string &from,
                          const std::string &to, const std::string &param,
                          const std::string &op, float threshold, float blend)
{
    Transition t;
    t.from = from;
    t.to = to;
    t.param = param;
    t.greater = (op != "<"); // 只有 "<" 是小于，其余按大于处理
    t.threshold = threshold;
    t.blend = blend;
    g_machines[id].transitions.push_back(std::move(t));
}

void animFsmStart(flecs::entity_t id, const std::string &state)
{
    auto &m = g_machines[id];
    m.state = state;
    m.started = true;
}

void animFsmSetParam(flecs::entity_t id, const std::string &param, float value)
{
    auto it = g_machines.find(id);
    if (it != g_machines.end())
        it->second.params[param] = value;
}

std::string animFsmState(flecs::entity_t id)
{
    auto it = g_machines.find(id);
    return (it != g_machines.end() && it->second.started) ? it->second.state : "";
}

void AnimFsmSystem::clearAll() { g_machines.clear(); }

void AnimFsmSystem::update(flecs::world &world, float /*dt*/)
{
    if (!isEnabled() || g_machines.empty())
        return;

    auto animCompId = world.component<AnimationComponent>().raw_id();

    for (auto it = g_machines.begin(); it != g_machines.end();)
    {
        flecs::entity e = world.entity(it->first);
        if (!e.is_alive())
        {
            it = g_machines.erase(it); // 实体没了：状态机作废
            continue;
        }
        Machine &m = it->second;
        if (!m.started)
        {
            ++it;
            continue;
        }

        auto *ac = (AnimationComponent *)ecs_get_mut_id(world.c_ptr(), it->first, animCompId);
        if (!ac)
        {
            ++it;
            continue; // 没挂动画组件：规则留着，等组件出现
        }

        // 状态机首帧/被外力改动（anim_play 直接切了 clip）→ 以状态机为准拉回
        if (strncmp(ac->current, m.state.c_str(), sizeof(ac->current)) != 0 &&
            ac->prev[0] == '\0') // 不在过渡中才纠正（过渡中 current 已是目标）
        {
            strncpy(ac->current, m.state.c_str(), sizeof(ac->current) - 1);
            ac->current[sizeof(ac->current) - 1] = '\0';
            ac->time = 0.0f;
            ac->playing = true;
        }

        // 判定当前状态的出边（第一条满足的生效——规则顺序即优先级）
        for (const auto &t : m.transitions)
        {
            if (t.from != m.state)
                continue;
            auto pIt = m.params.find(t.param);
            if (pIt == m.params.end())
                continue; // 参数还没喂过：不判定
            bool pass = t.greater ? (pIt->second > t.threshold)
                                  : (pIt->second < t.threshold);
            if (!pass)
                continue;

            // 触发切换：走 crossfade 混合管线（同 anim_crossfade 的写法）
            strncpy(ac->prev, ac->current, sizeof(ac->prev) - 1);
            ac->prev[sizeof(ac->prev) - 1] = '\0';
            ac->prevTime = ac->time;
            strncpy(ac->current, t.to.c_str(), sizeof(ac->current) - 1);
            ac->current[sizeof(ac->current) - 1] = '\0';
            ac->time = 0.0f;
            ac->blendTime = 0.0f;
            ac->blendDuration = t.blend > 0 ? t.blend : 0.0f;
            ac->playing = true;
            m.state = t.to;
            break; // 一帧最多切一次（防连锁跳变）
        }
        ++it;
    }
}
