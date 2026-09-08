#ifndef ANIMFSMSYSTEM_H
#define ANIMFSMSYSTEM_H

#include "Core/System.h"
#include <string>

// ============================================================
// AnimFsmSystem — 动画状态机（对标 Godot AnimationTree/StateMachine）
//
// 【解决什么】角色动画切换逻辑写在脚本里全是 if 面条：
//   if speed>0.5 and state=="idle" then anim_crossfade("walk") ... end
// 状态机把"什么时候切"变成声明式规则，脚本只喂参数：
//
//   -- ready() 里声明状态图（一次）：
//   anim_fsm_add(id, "idle", "walk", "speed", ">", 0.5, 0.2)
//   anim_fsm_add(id, "walk", "idle", "speed", "<", 0.1, 0.2)
//   anim_fsm_add(id, "walk", "run",  "speed", ">", 4.0, 0.15)
//   anim_fsm_add(id, "run",  "walk", "speed", "<", 3.5, 0.15)
//   anim_fsm_start(id, "idle")
//
//   -- update() 里只更新参数（切换自动发生，带 crossfade）：
//   anim_fsm_param(id, "speed", 当前速度)
//
// 【约定】状态名 = 动画 clip 名（Godot 同款默认）；切换用
//   AnimationComponent 的 crossfade 字段（prev/blend），无缝混合。
// 【数据存放】状态机是运行时结构（含 map，不适合 REFLECT 组件），
//   存在 System 内部（同 Tween 模式）；退 Play clearAll() 清空。
// ============================================================
class AnimFsmSystem : public System
{
public:
    int phase() const override { return 108; }           // 脚本(100)后、动画采样(110)前
    int activeIn() const override { return kAllStates; } // 无状态机时零开销

    void update(flecs::world &world, float dt) override;

    /// 退出 Play 清空全部状态机（残留规则会污染下次会话）
    static void clearAll();
};

// ---- 脚本入口（同 tweenStart 模式：全局函数投递，System 消费）----

/// 添加转换规则：处于 from 状态且 参数 op 阈值 成立 → crossfade 到 to
/// op: ">" 或 "<"；blend=过渡时长（秒）
void animFsmAddTransition(flecs::entity_t id, const std::string &from,
                          const std::string &to, const std::string &param,
                          const std::string &op, float threshold, float blend);

/// 设置初始状态（立即从头播放该 clip）
void animFsmStart(flecs::entity_t id, const std::string &state);

/// 更新参数值（切换判定在 AnimFsmSystem::update 里自动进行）
void animFsmSetParam(flecs::entity_t id, const std::string &param, float value);

/// 查询当前状态（脚本 anim_fsm_state 用；无状态机返回空串）
std::string animFsmState(flecs::entity_t id);

#endif
