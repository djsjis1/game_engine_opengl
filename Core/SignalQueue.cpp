// ============================================================
// SignalQueue.cpp — 多线程延迟信号队列实现（Core 层）
//
// 工作线程不能直接调 Lua（lua_State 非线程安全），
// 所以各系统发的信号先入队，主线程每帧消费（Scripting 层
// processDeferredSignals）。本文件只有队列/锁/投递/取发/清空 ——
// 零 Lua 依赖，这是它能放 Core 层的前提。
// ============================================================
#include "Core/SignalQueue.h"

#include <utility>

// 队列与锁文件私有：外部只能经 push/take/clear 三个接口访问（模块解耦铁律）
namespace
{
    std::queue<DeferredSignal> g_signalQueue;
    std::mutex g_signalMutex;
}

void pushDeferredSignal(DeferredSignal sig)
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    g_signalQueue.push(std::move(sig));
}

std::vector<DeferredSignal> takeDeferredSignals()
{
    std::queue<DeferredSignal> local;
    {
        std::lock_guard<std::mutex> lock(g_signalMutex);
        std::swap(local, g_signalQueue);
    }
    std::vector<DeferredSignal> out;
    out.reserve(local.size());
    while (!local.empty())
    {
        out.push_back(std::move(local.front()));
        local.pop();
    }
    return out;
}

void clearDeferredSignals()
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    std::queue<DeferredSignal> empty;
    std::swap(g_signalQueue, empty);
}
