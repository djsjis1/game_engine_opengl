#ifndef LUAASYNC_H
#define LUAASYNC_H

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

#include <string>
#include <queue>
#include <mutex>
#include "Core/JobSystem.h"

// ============================================================
// LuaAsync — Godot 风格的异步任务系统
//
// 脚本调用：
//   async([=[
//     local sum = 0
//     for i = 1, 1000000 do sum = sum + math.sin(i*0.001) end
//     return sum
//   ]=], function(result) print("Result: " .. result) end)
//
// 流程：
//   工作线程用自己的 lua_State 执行 func → 返回值入队
//   → 主线程 processDeferredSignals() 消费 → 调 callback
// ============================================================

struct AsyncTask
{
    std::string code; // Lua 代码（工作线程执行）
    int callbackRef;  // 回调在注册表中的 ref（主线程调用）
};

extern JobSystem g_jobSystem;
extern std::queue<AsyncTask> g_asyncResults;
extern std::mutex g_asyncMutex;

/// Lua API: async(code_string, callback)
int api_asyncRun(lua_State *L);

/// 初始化异步系统（ScriptSystem::init() 调用）
void initAsyncSystem(lua_State *mainL);

/// 消费异步结果（processDeferredSignals 之后调用）
void processAsyncResults(lua_State *L);

#endif
