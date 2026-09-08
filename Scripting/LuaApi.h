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

#include "Core/SignalQueue.h" // DeferredSignal/g_signalQueue（已抽到 Core 层，ADR-002）

// ============================================================
// LuaApi.h — Lua C API 的公共声明（供 ScriptEngine 引用）
//
// 本文件存放 LuaApi.cpp 和 ScriptEngine.cpp 之间共享的类型和函数声明。
// 信号队列的【定义】在 Core/SignalQueue.h/.cpp（底层系统只依赖 Core），
// 本头 include 它仅为兼容既有调用方。
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

/// 消费多线程信号队列（主线程每帧调用）
/// 队列本身在 Core/SignalQueue.h（生产者：物理/动画/补间，都在 Core/Systems 层）
void processDeferredSignals(lua_State *L, flecs::world &world);

/// 实体销毁时清理其信号连接（P1：防 Lua 注册表泄漏 + 孤儿回调）。
/// 由 Engine 的 SignalComponent OnRemove observer 调用——
/// ① unref 自己 signalMap 里所有监听者回调
/// ② 用 listensTo 去各源实体的 signalMap 删掉"listenerId==自己"的连接并 unref
/// 放 LuaApi（能访问 lua_State）而非组件层，保持 Components 不依赖 Lua。
void cleanupEntitySignals(flecs::world world, flecs::entity_t deadId);

/// 脚本 VM 关闭前调用（ScriptSystem::shutdown）：置空 LuaApi 记录的主 VM
/// 指针。否则 lua_close 后实体销毁触发 cleanupEntitySignals → 对已关闭
/// 的 state luaL_unref → 崩溃（审查 C1：悬空全局指针）
void invalidateApiLuaState();

/// 每帧推进 delay() 定时器（ScriptSystem::update 调用）
void processTimers(lua_State *L, float dt);

/// 清空未触发的定时器并 unref 回调（退出 Play / resetAllScripts 时）
void clearTimers(lua_State *L);

/// 编译缓存（脚本字节码缓存，LuaApi.cpp 和 ScriptEngine.cpp 共享）
extern std::map<std::string, CachedChunk> g_compiledChunks;

/// 主线程 ID（Signal init 设置，api_emit 判断用）
extern std::thread::id g_mainThreadId;

/// 测试钩子：设置后 ScriptSystem::init() 时注入额外 Lua API（主程序不用，为 null）
using ScriptApiHook = void (*)(lua_State *);
extern ScriptApiHook g_extraApiHook;
inline void setScriptExtraApiHook(ScriptApiHook fn) { g_extraApiHook = fn; }

#endif
