#ifndef ENGINE_LOG_H
#define ENGINE_LOG_H

#include <functional>
#include <string>

// ============================================================
// EngineLog — 引擎日志出口（脚本错误/系统警告的统一通道）
//
// 【两条腿】engineLogError() 同时走：
//   ① mylog（thirdparty/log 的 spdlog 封装）→ 彩色控制台 + logs/ 滚动
//      文件（线程安全、带时间戳源码位置，首次调用自动初始化）
//   ② UI sink（可选）→ 编辑器"输出"面板实时显示（Godot Output 同款）
//
// 【解耦】本头零重依赖（纯 std）：spdlog 头只在 EngineLog.cpp 内部
//   include，下游（ScriptEngine 等）include 本头不拉 spdlog。
//   UI sink 用 std::function 注册——换 UI 框架时引擎层零改动。
// ============================================================

/// UI 层注册日志接收器（如编辑器输出面板）。传 nullptr 取消
void setEngineLogSink(std::function<void(const std::string &)> fn);

/// 引擎错误日志：mylog 落盘+控制台，有 UI sink 再发一份给面板
void engineLogError(const std::string &msg);

/// 进程退出前调用（main 返回前）：flush + 释放 spdlog。
/// 不调的话 spdlog 静态对象析构顺序不定，退出时可能 0xC0000005
void shutdownEngineLog();

#endif
