// ============================================================
// EngineLog.cpp — 引擎日志实现（唯一 include mylog/spdlog 的地方）
//
// 隔离哲学：spdlog 的重头文件只在本 .cpp 出现（同 AudioEngine 隔离
// miniaudio、PhysicsEngine 隔离 Jolt）。换日志后端只改本文件。
// ============================================================
#include "Core/EngineLog.h"
#include "Logger.h" // thirdparty/log 的 mylog 封装（spdlog）

namespace
{
    // UI 日志接收器（编辑器输出面板注册；导出的 my_game 无 UI 不注册）
    std::function<void(const std::string &)> g_uiSink;

    // mylog 懒初始化：首次日志时 init（logs/engine.log 滚动 + 彩色控制台）。
    // 不在 main 里强制 init——引擎层不该要求宿主"记得初始化日志"
    mylog::Logger &engineLogger()
    {
        static bool inited = []
        {
            mylog::LogConfig cfg;
            cfg.logger_name = "engine";
            cfg.log_file = "engine.log"; // logs/engine.log（工作目录旁）
            cfg.level = mylog::Level::Info;
            return mylog::SingletonLogger::init(cfg);
        }();
        (void)inited;
        return mylog::SingletonLogger::instance();
    }
} // namespace

void setEngineLogSink(std::function<void(const std::string &)> fn)
{
    g_uiSink = std::move(fn);
}

void engineLogError(const std::string &msg)
{
    // ① mylog：彩色控制台 + logs/ 滚动文件（线程安全）
    LOG_ERROR(engineLogger(), "{}", msg);
    // ② UI 面板（如已注册）：编辑器实时可见
    if (g_uiSink)
        g_uiSink(msg);
}

void shutdownEngineLog()
{
    g_uiSink = nullptr; // UI 可能先于日志销毁，退出期不再回调
    mylog::SingletonLogger::shutdown();
}
