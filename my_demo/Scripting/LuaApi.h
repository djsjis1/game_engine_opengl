#ifndef LUAAPI_H
#define LUAAPI_H

#pragma push_macro("LUA_API")
#pragma push_macro("LUAI_FUNC")
#pragma push_macro("LUALIB_API")
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#pragma pop_macro("LUALIB_API")
#pragma pop_macro("LUAI_FUNC")
#pragma pop_macro("LUA_API")

#include <flecs.h>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <ctime>
#include <thread>

// ============================================================
// LuaApi.h — Lua C API 的公共声明（供 ScriptEngine 引用）
//
// 本文件存放 LuaApi.cpp 和 ScriptEngine.cpp 之间共享的类型和函数声明。
// ============================================================

/// 编译缓存条目
struct CachedChunk
{
    std::string data; // 编译后的字节码
    time_t mtime = 0; // 源文件的最后修改时间
};

/// 当前实体的上下文（从注册表传递）
struct LuaCtx
{
    struct TransformComponent *tf; // Transform 指针（零拷贝）
    flecs::world *w;               // ECS 世界指针
    flecs::entity_t id;            // 当前实体 ID
};

/// 从 Lua 注册表取出当前实体的上下文
LuaCtx *getCtx(lua_State *L);

/// 把 C++ API 函数注入到 Lua 沙盒 table 中（set_position, emit, ...）
void injectApi(lua_State *L);

/// 多线程信号队列条目（C++ 工作线程/物理碰撞直接推此结构，主线程消费）
/// args 用 double：Lua number 本身是 double，且能完整承载实体 ID
struct DeferredSignal
{
    flecs::entity_t emitterId;
    std::string signalName;
    std::vector<double> args;
};

/// 消费多线程信号队列（主线程每帧调用）
void processDeferredSignals(lua_State *L, flecs::world &world);

/// 清空信号队列（丢弃未消费信号）
/// 用途：进/出 Play 时调用 —— 编辑模式预览动画产生的事件信号
/// 没有消费者（ScriptSystem 仅 Play 活跃），不清会堆积，
/// 进 Play 瞬间陈旧信号集中轰炸脚本回调
void clearDeferredSignals();

/// 信号队列 & 锁（供物理碰撞监听等后台线程推送信号）
extern std::queue<DeferredSignal> g_signalQueue;
extern std::mutex g_signalMutex;

/// 编译缓存（脚本字节码缓存，LuaApi.cpp 和 ScriptEngine.cpp 共享）
extern std::map<std::string, CachedChunk> g_compiledChunks;

/// 主线程 ID（Signal init 设置，api_emit 判断用）
extern std::thread::id g_mainThreadId;

#endif
