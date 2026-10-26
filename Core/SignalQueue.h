#ifndef SIGNALQUEUE_H
#define SIGNALQUEUE_H

// emit 宏保护：Qt 定义了 emit 宏，会破坏 flecs 源码里的 emit 变量名（MODULES.md 坑1）
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include <string>
#include <vector>
#include <queue>
#include <mutex>

// ============================================================
// SignalQueue — 多线程延迟信号队列（Core 层总线）
//
// 【为什么在 Core 层】
//   三个底层系统都要往脚本层发信号，但底层不该依赖脚本层：
//     PhysicsEngine（碰撞开始/结束）→ 曾 include Scripting/LuaApi.h ✗
//     AnimationSystem（动画事件轨道）→ 曾 include Scripting/LuaApi.h ✗
//     TweenSystem（补间完成信号）   → 曾 include Scripting/LuaApi.h ✗
//   把队列抽到 Core 后，三者只依赖 Core/SignalQueue.h，
//   Scripting 层每帧来消费 —— 依赖方向统一指向 Core（ADR-002）。
//
// 【线程模型】
//   生产者：物理接触监听跑在 Jolt 工作线程、其余在主线程 —— 内部加锁
//   消费者：ScriptSystem 主线程每帧 takeDeferredSignals()（Scripting 层）
//   lua_State 非线程安全，工作线程绝不直接调 Lua，一律先入队。
//
// 【封装】
//   队列与锁是文件私有状态（匿名命名空间），头文件只暴露三个接口：
//   push / take / clear。外部拿不到裸队列，不存在绕过锁的路径。
// ============================================================

/// 延迟信号条目（C++ 系统推送，主线程消费后调 Lua 回调）
/// args 用 double：Lua number 本身是 double，且能完整承载实体 ID
struct DeferredSignal
{
    flecs::entity_t emitterId;
    std::string signalName;
    std::vector<double> args;
};

/// 线程安全的信号投递入口（生产者唯一写入口）
void pushDeferredSignal(DeferredSignal sig);

/// 原子取出全部未消费信号（加锁 swap，消费方唯一读入口）
std::vector<DeferredSignal> takeDeferredSignals();

/// 清空信号队列（丢弃未消费信号）
/// 用途：进/出 Play 时调用 —— 编辑模式预览动画产生的事件信号
/// 没有消费者（ScriptSystem 仅 Play 活跃），不清会堆积，
/// 进 Play 瞬间陈旧信号集中轰炸脚本回调
void clearDeferredSignals();

#endif
