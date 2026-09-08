// ============================================================
// LuaAsync.cpp — Godot 风格异步任务实现
//
// 架构：
//   工作线程有独立 lua_State（只有 math/string/table 库，不能访问游戏对象）
//   主线程的 lua_State 有完整 API（get_position, emit, ...）
// ============================================================

#include "LuaAsync.h"
#include "Core/JobSystem.h"
#include <cstring>
#include <queue>
#include <mutex>

// ---- 异步系统内部状态（文件私有，外部通过头文件三个函数访问）----
namespace
{
    // 工作线程结果 → 主线程回调。
    // ★ 无论成功失败都要入队（携带 callbackRef），主线程统一 unref，
    //   否则失败路径的注册表引用永久泄漏（见修复记录）。
    struct AsyncTask
    {
        double result = 0.0;  // 工作线程返回的数字（失败时无意义）
        bool ok = false;      // 执行成功？失败则主线程以 nil 调回调
        int callbackRef = -1; // 回调在注册表中的 ref（主线程调用后 unref）
    };

    JobSystem g_jobSystem(4);
    std::queue<AsyncTask> g_asyncResults;
    std::mutex g_asyncMutex;
} // namespace

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
        // 失败也要把结果入队（带 cbRef），否则主线程永远不会 unref → 泄漏
        auto pushResult = [cbRef](double value, bool ok) {
            std::lock_guard<std::mutex> lock(g_asyncMutex);
            g_asyncResults.push({value, ok, cbRef});
        };

        lua_State *wL = createWorkerState();

        // 工作线程执行 Lua 代码
        if (luaL_loadstring(wL, code.c_str()) != LUA_OK)
        {
            lua_close(wL);
            pushResult(0.0, false); // 编译失败：回调收 nil
            return;
        }
        if (lua_pcall(wL, 0, 1, 0) != LUA_OK)
        {
            lua_close(wL);
            pushResult(0.0, false); // 运行出错：回调收 nil
            return;
        }

        // 成功：直接取数字回传（非数字结果本系统按 0 处理——异步只支持数值计算）
        double result = lua_tonumber(wL, -1);
        lua_close(wL);
        pushResult(result, true); });

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

        // 直接把工作线程算好的数字（或失败时 nil）传给回调——
        // 不再"重新编译 return N 字符串"（去掉每结果一次 luaL_loadstring）
        if (task.ok)
            lua_pushnumber(L, task.result);
        else
            lua_pushnil(L); // 工作线程编译/运行失败：回调收 nil
        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            lua_pop(L, 1); // 回调自身报错：弹掉错误消息，不影响后续任务

        luaL_unref(L, LUA_REGISTRYINDEX, task.callbackRef);
        local.pop();
    }
}

void initAsyncSystem(lua_State *) {}
