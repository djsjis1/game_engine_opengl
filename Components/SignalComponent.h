#ifndef SIGNALCOMPONENT_H
#define SIGNALCOMPONENT_H

#include <map>
#include <vector>
#include <string>

#pragma push_macro("emit")
#undef emit
#pragma push_macro("signals")
#undef signals
#include <flecs.h>
#pragma pop_macro("signals")
#pragma pop_macro("emit")

// ============================================================
// SignalComponent — 实体级信号系统
//
// 每个实体一份，挂在 ECS 中。callbackRef 是 Lua 注册表引用，实体销毁时
// 由 Engine 的 SignalComponent OnRemove observer 调 cleanupEntitySignals
// 逐个 luaL_unref（signalMap 里别人监听我的 + listensTo 去源实体清我的）。
// ★ 光靠 ECS 删组件【不会】回收这些 Lua ref——必须走 observer，否则泄漏
//   注册表槽位 + 孤儿回调（源实体 emit 会调到已死监听者）。
//
// 【它解决什么问题】
//   实体 A 想在"某事发生时"通知实体 B，但 A 不需要知道 B 是谁。
//   A 只管 emit("hit", 50)，B 只管 connect("hit", callback)。
//
// 【两个成员各管一个方向】
//   signalMap  — "谁在监听我的信号？"（我发信号时查这个）
//   listensTo  — "我监听了谁？"（我被删时用它去别人那清理）
//
// 【具体例子】
//   场景：Player(id=42) 监听 Enemy(id=99) 的 "death" 信号
//
//   Enemy(99) 的 SignalComponent：
//     signalMap = {
//       "death" → [{listenerId:42, callbackRef:5}]
//     }
//     含义："death" 信号有 1 个监听者——实体 42，回调在注册表第 5 槽。
//     当 Enemy 调用 emit("death", 50) 时，遍历这条列表，逐一调用回调。
//
//   Player(42) 的 SignalComponent：
//     listensTo = {
//       99 → ["death"]
//     }
//     含义：我(Player)监听了实体 99 的 "death" 信号。
//     当我被删除时，根据这条记录去 Enemy.signalMap["death"] 中
//     清理 {listenerId:42, ...}，防止残留孤儿回调。
//
// 【Lua 端对应 API】
//   connect("hit", cb)        → 写入自己的 signalMap["hit"]
//   emit("hit", 50)           → 遍历自己的 signalMap["hit"]，调所有回调
//   connect_to(B_id, "h", cb) → 写入 B 的 signalMap["h"] + 自己的 listensTo
//   disconnect("hit" [, ref]) → 从 signalMap 删条目 + 释放注册表回调
// ============================================================

struct SignalConnection
{
    flecs::entity_t listenerId; // 谁在监听（实体 ID）
    int callbackRef = 0;        // 回调在 Lua 注册表的槽位（luaL_ref 返回值）
};

struct SignalComponent
{
    // 【正向】我发信号 → 通知谁
    // key = 信号名（"hit", "death", "damaged" ...）
    // value = 监听者列表（可能有多个实体同时监听同一个信号）
    std::map<std::string, std::vector<SignalConnection>> signalMap;

    // 【反向】我被删 → 去别人那清理我的监听记录
    // key = 被监听的源实体 ID
    // value = 我监听了它的哪些信号
    std::map<flecs::entity_t, std::vector<std::string>> listensTo;
};

#endif