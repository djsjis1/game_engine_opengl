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
//
// 【解耦】线程池/结果队列/锁/AsyncTask 全是实现细节，封在 .cpp 匿名
//   命名空间里（文件私有）——外部只用下面三个函数，碰不到内部状态。
//   （与 Input/SaveGame/SceneRouter 一致：私有数据 + 只暴露接口）
// ============================================================

/// Lua API: async(code_string, callback)
int api_asyncRun(lua_State *L);

/// 初始化异步系统（ScriptSystem::init() 调用）
void initAsyncSystem(lua_State *mainL);

/// 消费异步结果（processDeferredSignals 之后调用）
void processAsyncResults(lua_State *L);

#endif
