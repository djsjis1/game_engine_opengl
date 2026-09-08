#ifndef ENGINEFSM_H
#define ENGINEFSM_H

#include <functional>

#define HFSM2_ENABLE_STRUCTURE_REPORT
#include <hfsm2/machine.hpp>

// ============================================================
// EngineContext — 状态间共享回调（避免循环依赖 Engine.h）
//
// MainWindow 在构造时注册这些回调，
// HFSM2 状态 enter/exit 时自动调用。
// ============================================================
struct EngineContext
{
    std::function<void()> onEnterEdit;
    std::function<void()> onExitEdit;
    std::function<void()> onEnterPlay;
    std::function<void()> onExitPlay;
};

// ★ ContextT<EngineContext &> 必须是引用！（实际踩过的坑）
// 值语义 ContextT<EngineContext> 会让 FSM 构造时拷贝 context：
//   Engine 构造 FSM 时回调还全是空 → MainWindow 稍后注册的回调
//   写进了 Engine 的原件，FSM 内部那份拷贝永远是空回调
//   → 状态切换正常但 enter/exit 什么都不做（症状：运行模式动画不播）
using M = hfsm2::MachineT<hfsm2::Config::ContextT<EngineContext &>>;

#define S(s) struct s
using FSM = M::PeerRoot<
    S(EditState),
    S(PlayState)>;
#undef S

// ============================================================
// EditState — 编辑器模式
// 方法签名用官方示例同款 enter(Control&) / exit(Control&)
// ============================================================
struct EditState : FSM::State
{
    void enter(Control &control)
    {
        if (control.context().onEnterEdit)
            control.context().onEnterEdit();
    }
    void exit(Control &control)
    {
        if (control.context().onExitEdit)
            control.context().onExitEdit();
    }
};

// ============================================================
// PlayState — 运行模式
// ============================================================
struct PlayState : FSM::State
{
    void enter(Control &control)
    {
        if (control.context().onEnterPlay)
            control.context().onEnterPlay();
    }
    void exit(Control &control)
    {
        if (control.context().onExitPlay)
            control.context().onExitPlay();
    }
};

#endif
