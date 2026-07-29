// ============================================================
// LuaAsync.cpp — Godot 风格异步任务实现
//
// 架构：
//   工作线程有独立 lua_State（只有 math/string/table 库，不能访问游戏对象）
//   主线程的 lua_State 有完整 API（get_position, emit, ...）
// ============================================================

#include "LuaAsync.h"
#include <cstring>

JobSystem g_jobSystem(4);
std::queue<AsyncTask> g_asyncResults;
std::mutex g_asyncMutex;

// ---- 工作线程的 Lua 环境 ----
static lua_State *createWorkerState()
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L); // 工作线程有 math、string、table，但没有 get_position/emit 等
    return L;
}

/// Lua API：主线程调用，把任务提交到线程池
int api_asyncRun(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TSTRING);   // 参数1：Lua 代码字符串
    luaL_checktype(L, 2, LUA_TFUNCTION); // 参数2：回调函数

    std::string code = lua_tostring(L, 1);

    // 保存回调到注册表
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // ★ 提交到线程池
    g_jobSystem.submit([code, cbRef]()
                       {
        lua_State *wL = createWorkerState();

        // 工作线程执行 Lua 代码
        if (luaL_loadstring(wL, code.c_str()) != LUA_OK)
        {
            lua_close(wL);
            return; // 编译失败，静默忽略
        }

        // pcall 执行，期望返回 1 个值
        if (lua_pcall(wL, 0, 1, 0) != LUA_OK)
        {
            lua_close(wL);
            return;
        }

        // 把返回值推到主线程队列
        double result = lua_tonumber(wL, -1);
        lua_close(wL);

        // 构造虚拟 code —— 把 result 推给回调
        std::string resultCode = "return " + std::to_string(result);

        std::lock_guard<std::mutex> lock(g_asyncMutex);
        g_asyncResults.push({std::move(resultCode), cbRef}); });

    return 0;
}

/// 消费异步结果（主线程每帧调用）
void processAsyncResults(lua_State *L)
{
    std::queue<AsyncTask> local;
    {
        std::lock_guard<std::mutex> lock(g_asyncMutex);
        std::swap(local, g_asyncResults);
    }

    // 异步回调无实体上下文，清空 __active_ctx（防止读到上帧悬空指针）
    lua_pushstring(L, "__active_ctx");
    lua_pushlightuserdata(L, nullptr);
    lua_settable(L, LUA_REGISTRYINDEX);

    while (!local.empty())
    {
        auto &task = local.front();

        // 取出回调
        lua_rawgeti(L, LUA_REGISTRYINDEX, task.callbackRef);
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_unref(L, LUA_REGISTRYINDEX, task.callbackRef);
            local.pop();
            continue;
        }

        // 执行 task.code 获取工作线程的返回值
        if (luaL_loadstring(L, task.code.c_str()) == LUA_OK)
        {
            lua_pcall(L, 0, 1, 0); // 执行后栈上有 1 个返回值
            lua_pcall(L, 1, 0, 0); // 把返回值传给回调
        }
        else
        {
            lua_pop(L, 1); // 弹出编译失败的错误
            lua_pushnil(L);
            lua_pcall(L, 1, 0, 0);
        }

        luaL_unref(L, LUA_REGISTRYINDEX, task.callbackRef);
        local.pop();
    }
}

void initAsyncSystem(lua_State *) {}
