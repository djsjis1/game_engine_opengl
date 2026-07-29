#ifndef ENGINESTATE_H
#define ENGINESTATE_H

// ============================================================
// EngineState — 引擎运行状态（替代散落的 bool flag）
//
// 当前两个模式：
//   Edit — 编辑器自由飞摄像机，不运行脚本
//   Play — 脚本启动，编辑器隐藏
//
// 未来可扩展：Pause、PlayStart、PlayStop 等过渡态。
// 所有 System 通过 activeIn() 声明自己在哪些状态下运行。
// ============================================================
enum class EngineState : int
{
    Edit = 1 << 0,
    Play = 1 << 1,
};

// 位掩码：System 可以同时活跃于多个状态
inline constexpr int kAllStates = (int)EngineState::Edit | (int)EngineState::Play;

#endif
