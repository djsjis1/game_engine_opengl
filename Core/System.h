#ifndef SYSTEM_H
#define SYSTEM_H

#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include "EngineState.h"

// ============================================================
// System — 所有系统的基类
//
// pipeline 机制：
//   phase()   — 返回执行阶段编号，Engine 按 phase 升序执行
//   activeIn() — 返回位掩码，声明本系统在哪些 EngineState 下活跃
//
// 示例：
//   class RotationSystem : public System {
//       int  phase()    override { return 100; }
//       int  activeIn() override { return kAllStates; }
//       void update(world, dt) override { ... }
//   };
// ============================================================
class System
{
public:
    virtual ~System() = default;

    /// 执行阶段（数字越小越先执行）
    /// 默认 0，可覆写。Camera=50, Script=100, Render=200
    virtual int phase() const { return 0; }

    /// 活跃状态位掩码（EngineState::Edit | EngineState::Play）
    /// 默认所有状态都执行
    virtual int activeIn() const { return kAllStates; }

    /// 每帧由 Engine 调用
    virtual void update(flecs::world &world, float dt) = 0;

    /// 引擎销毁前调用（ScriptSystem 在此释放 lua_State）
    virtual void shutdown() {}

    /// 由 Engine 调用，标记此 System 是否被当前管线激活
    void setEnabled(bool e) { m_enabled = e; }
    bool isEnabled() const { return m_enabled; }

private:
    bool m_enabled = true;
};

#endif
