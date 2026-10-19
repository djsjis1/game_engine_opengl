// ============================================================
// ScriptEngine.cpp — ScriptSystem 实现（Lua VM 管理 + 生命周期）
//
// 负责：创建 Lua VM、为每个实体初始化沙盒环境、每帧调用 ready/update
// Lua API 函数本身在 LuaApi.cpp 中
// ============================================================

#include "LuaApi.h"
#include "LuaAsync.h"
#include "Systems/ScriptSystem.h"
#include "Components/ScriptComponent.h"
#include "Components/TransformComponent.h"
#include "Core/EngineLog.h" // 脚本错误 → 编辑器输出面板（无 UI 时落 stderr）

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdio> // fprintf：脚本错误日志（M4）
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <optional>

// ============================================================
// 脚本加载与编译
// ============================================================

/// 读取脚本源文件内容
static std::string loadSource(const ScriptComponent &sc)
{
    if (sc.filePath[0])
    {
        std::ifstream f(sc.filePath);
        if (f.is_open())
        {
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return sc.source[0] ? std::string(sc.source) : "";
}

/// 编译（或加载缓存）脚本 → 存入注册表
static bool compileAndLoad(lua_State *L, const ScriptComponent &sc, int *outRef)
{
    std::string key = sc.filePath[0] ? sc.filePath : ("inline:" + std::string(sc.source));
    bool needRecompile = true;
    if (sc.filePath[0])
    {
        struct stat st;
        if (stat(sc.filePath, &st) == 0)
        {
            auto it = g_compiledChunks.find(key);
            if (it != g_compiledChunks.end() && it->second.mtime == st.st_mtime)
                needRecompile = false;
        }
    }
    if (needRecompile)
    {
        std::string src = loadSource(sc);
        if (src.empty())
            return false;
        if (luaL_loadstring(L, src.c_str()) != LUA_OK)
        {
            // 编译错误必须出声（带文件名，直接可定位）——错误静默是排障噩梦
            engineLogError(std::string("[Script] 编译失败 ") +
                           (sc.filePath[0] ? sc.filePath : "(内嵌脚本)") +
                           ": " + lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        // 文件脚本：dump 字节码进缓存。★ dump 不消费栈顶 chunk，
        //   所以 dump 后直接复用栈上这份编译结果（不再二次 luaL_loadstring）
        if (sc.filePath[0])
        {
            CachedChunk cc;
            lua_dump(L, [](lua_State *, const void *p, size_t sz, void *ud) -> int
                     { ((CachedChunk*)ud)->data.append((const char*)p,sz); return 0; }, &cc, 0);
            struct stat st;
            cc.mtime = stat(sc.filePath, &st) == 0 ? st.st_mtime : 0;
            g_compiledChunks[key] = std::move(cc);
            // 栈顶仍是 loadstring 编译好的 chunk → 下方直接 luaL_ref 复用
        }
    }
    else
    {
        auto &cc = g_compiledChunks[key];
        if (luaL_loadbuffer(L, cc.data.data(), cc.data.size(), key.c_str()) != LUA_OK)
        {
            lua_pop(L, 1);
            return false;
        }
    }
    *outRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return true;
}

/// 为实体初始化 Lua 沙盒环境（每个实体只执行一次）
/// 详见 LuaApi.cpp 顶部注释
static bool initEntity(lua_State *L, const ScriptComponent &sc, int &outEnvRef)
{
    lua_newtable(L);
    lua_newtable(L);
    lua_getglobal(L, "_G");
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    injectApi(L);

    outEnvRef = luaL_ref(L, LUA_REGISTRYINDEX);

    int funcRef;
    if (!compileAndLoad(L, sc, &funcRef))
        return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, funcRef);
    lua_rawgeti(L, LUA_REGISTRYINDEX, outEnvRef);
    lua_setupvalue(L, -2, 1);

    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        // 脚本主体执行错误（顶层代码就崩了，ready/update 都不会有）
        engineLogError(std::string("[Script] 主体执行失败 ") +
                       (sc.filePath[0] ? sc.filePath : "(内嵌脚本)") +
                       ": " + lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, funcRef);
    return true;
}

/// 调用沙盒环境中的生命周期函数。
/// hasDt 显式声明"要不要传 dt 参数"——不能用 dt>0 判断！
/// （曾是实 bug：帧间隔恰好 0ms 时 update 收到 nil，脚本算术崩）
static void callLifecycle(lua_State *L, int envRef, const char *name,
                          bool hasDt = false, float dt = 0)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 2);
        return;
    }
    if (hasDt)
        lua_pushnumber(L, dt); // dt=0 也如实传 0（Lua 端永远拿到数字）
    if (lua_pcall(L, hasDt ? 1 : 0, 0, 0) != LUA_OK)
    {
        // 运行错误必须出声——静默丢弃让脚本 bug 无从排查
        engineLogError(std::string("[Script] ") + name + "() 运行错误: " +
                       lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// ============================================================
// ScriptSystem 实现
// ============================================================

struct ScriptSystem::Impl
{
    lua_State *L = nullptr;
    std::map<flecs::entity_t, int> envRefs;
    std::map<flecs::entity_t, bool> wasReady;
    std::map<flecs::entity_t, time_t> scriptMtimes;
    flecs::observer observer;
    flecs::world *world = nullptr; // 保存 world（reloadScript 遍历实体用）
    float hotReloadSeconds = 0.0f;
};

ScriptSystem::~ScriptSystem() = default;

void ScriptSystem::init(flecs::world &world)
{
    m_impl = new Impl;
    m_impl->L = luaL_newstate();
    luaL_openlibs(m_impl->L);
    m_impl->world = &world; // reloadScript 需要遍历实体

    g_mainThreadId = std::this_thread::get_id();

    // 测试钩子：无头测试通过它注入 test_assert 等 API（主程序不设置则为 null）
    if (g_extraApiHook)
        g_extraApiHook(m_impl->L);

    m_impl->observer = world.observer<ScriptComponent>()
                           .event(flecs::OnRemove)
                           .each([this](flecs::entity e, ScriptComponent &)
                                 {
            if (!m_impl || !m_impl->L) return;
            flecs::entity_t id = e.raw_id();
            auto refIt = m_impl->envRefs.find(id);
            if (refIt != m_impl->envRefs.end())
            {
                luaL_unref(m_impl->L, LUA_REGISTRYINDEX, refIt->second);
                m_impl->envRefs.erase(refIt);
            }
            m_impl->wasReady.erase(id);
            m_impl->scriptMtimes.erase(id); });
}

void ScriptSystem::shutdown()
{
    if (m_impl)
    {
        // Engine destroys systems before its world. Remove the observer while
        // this object is still alive; otherwise world teardown calls a lambda
        // that captured a deleted ScriptSystem.
        if (m_impl->observer.is_alive())
            m_impl->observer.destruct();
        if (m_impl->L)
        {
            invalidateApiLuaState(); // C1：先置空 LuaApi 的全局 VM 指针，
                                     // 防 lua_close 后实体销毁触发
                                     // cleanupEntitySignals 操作已关闭的 state
            lua_close(m_impl->L);
        }
        delete m_impl;
        m_impl = nullptr;
    }
}

void ScriptSystem::update(flecs::world &world, float dt)
{
    if (!isEnabled() || !m_impl || !m_impl->L)
        return;
    lua_State *L = m_impl->L;

    processDeferredSignals(L, world);
    processAsyncResults(L);
    processTimers(L, dt); // delay() 定时器推进（到点调回调）

    // External edits need only a short polling cadence. The built-in editor
    // calls reloadScript immediately, and exported games disable this path.
    bool pollExternalEdits = false;
    if (m_hotReloadMode != HotReloadMode::Disabled)
    {
        m_impl->hotReloadSeconds += dt;
        if (m_impl->hotReloadSeconds >= 0.5f)
        {
            m_impl->hotReloadSeconds = 0.0f;
            pollExternalEdits = true;
        }
    }
    std::map<std::string, std::optional<time_t>> checkedMtimes;

    // ★ 两阶段执行（Godot 同款）：先收集实体，遍历结束后再跑脚本。
    //   直接在 world.each 里调 Lua 的话，脚本一旦做结构变更（connect_to
    //   给目标实体加 SignalComponent / spawn / destruct），flecs 正在迭代
    //   的表被锁 → assert(LOCKED_STORAGE) 直接崩（集成测试实锤）。
    //   阶段二里每实体重新取组件指针——阶段一的指针可能因结构变更失效。
    std::vector<flecs::entity_t> scripted;
    world.each([&](flecs::entity e, TransformComponent &, ScriptComponent &)
               { scripted.push_back(e.raw_id()); });

    auto tfCompId = world.component<TransformComponent>().raw_id();
    auto scCompId = world.component<ScriptComponent>().raw_id();

    for (auto id : scripted)
    {
        flecs::entity e = world.entity(id);
        // 脚本可能销毁了后续实体（如子弹命中删敌人）——逐个复查
        if (!e.is_alive())
            continue;
        auto *tf = (TransformComponent *)ecs_get_mut_id(world.c_ptr(), id, tfCompId);
        auto *sc = (ScriptComponent *)ecs_get_mut_id(world.c_ptr(), id, scCompId);
        if (!tf || !sc)
            continue; // 组件被脚本移除

        auto it = m_impl->envRefs.find(id);
        if (it == m_impl->envRefs.end())
        {
            int envRef;
            if (!initEntity(L, *sc, envRef))
                continue;
            m_impl->envRefs[id] = envRef;
            m_impl->wasReady[id] = false;
            if (sc->filePath[0])
            {
                struct stat st;
                if (stat(sc->filePath, &st) == 0)
                    m_impl->scriptMtimes[id] = st.st_mtime;
            }
            it = m_impl->envRefs.find(id);
        }
        int envRef = it->second;

        if (sc->filePath[0] && pollExternalEdits)
        {
            // 外部编辑器修改文件时兜底；内置编辑器保存直接调用 reloadScript。
            auto [checked, inserted] = checkedMtimes.try_emplace(sc->filePath);
            if (inserted)
            {
                struct stat st;
                if (stat(sc->filePath, &st) == 0)
                    checked->second = st.st_mtime;
            }
            if (checked->second)
            {
                auto mtimeIt = m_impl->scriptMtimes.find(id);
                if (mtimeIt != m_impl->scriptMtimes.end() && mtimeIt->second != *checked->second)
                {
                    int oldEnvRef = envRef;
                    int newEnvRef;
                    if (initEntity(L, *sc, newEnvRef))
                    {
                        luaL_unref(L, LUA_REGISTRYINDEX, oldEnvRef);
                        m_impl->envRefs[id] = newEnvRef;
                        m_impl->wasReady[id] = false;
                        m_impl->scriptMtimes[id] = *checked->second;
                        envRef = newEnvRef;
                    }
                    else
                    {
                        m_impl->scriptMtimes[id] = *checked->second;
                        continue;
                    }
                }
            }
        }

        LuaCtx ctx = {tf, &world, id};
        lua_pushstring(L, "__active_ctx");
        lua_pushlightuserdata(L, &ctx);
        lua_settable(L, LUA_REGISTRYINDEX);

        if (!m_impl->wasReady[id])
        {
            callLifecycle(L, envRef, "ready");
            m_impl->wasReady[id] = true;
        }
        callLifecycle(L, envRef, "update", true, dt); // 恒传 dt（哪怕 0）
    }

    // H3：循环结束后 __active_ctx 指向最后一个 ctx（栈变量，已出作用域）。
    // 置空防止帧内后续路径（信号/异步回调）getCtx 拿到悬空指针读脏数据
    lua_pushstring(L, "__active_ctx");
    lua_pushlightuserdata(L, nullptr);
    lua_settable(L, LUA_REGISTRYINDEX);
}

// ============================================================
// 热重载：主动重置指定路径的脚本（内置编辑器保存后调用）
//
// 为什么不能只靠 update 里的 mtime 轮询：
//   st_mtime 是整秒精度，同一秒内保存检测不到变化（Godot 靠主动通知）。
// 做法：清编译缓存（强制下次重编）+ 释放匹配实体的 envRef
//   → 下帧 update 发现 envRefs 无此实体 → 重新 initEntity + 重跑 ready()
// ============================================================
void ScriptSystem::reloadScript(const std::string &path)
{
    if (!m_impl || !m_impl->L || !m_impl->world || path.empty())
        return;
    lua_State *L = m_impl->L;

    // 1. 清掉该文件的字节码缓存（键就是 filePath），强制重编译
    g_compiledChunks.erase(path);

    // 2. 释放匹配该路径的实体环境 → 下帧自然重建
    m_impl->world->each([&](flecs::entity e, ScriptComponent &sc)
                        {
        if (path != sc.filePath) return;
        flecs::entity_t id = e.raw_id();
        auto refIt = m_impl->envRefs.find(id);
        if (refIt != m_impl->envRefs.end())
        {
            luaL_unref(L, LUA_REGISTRYINDEX, refIt->second);
            m_impl->envRefs.erase(refIt); // 下帧 update 会重新 initEntity
        }
        m_impl->wasReady.erase(id);       // 重跑 ready()
        m_impl->scriptMtimes.erase(id); });
}

// ============================================================
// 重置全部脚本环境（退出 Play 时调用，对标 Godot"停止即销毁场景状态"）
//
// 为什么必须做：脚本环境跨 Play 会话复用的话——
//   ready() 只跑一次（wasReady 永久 true）→ 第二次 Play 时
//   set_mouse_mode/set_visible/局部变量初始化全部缺失 →
//   "第一次运行正常，第二次视角不正常"（实bug：鼠标不锁定+看到自己方块）
// 做法与 reloadScript 相同（释放 envRef → 下帧 update 自动重建），
// 只是范围是"全部实体"。字节码缓存保留（源码没变，无需重编译）。
// ============================================================
void ScriptSystem::resetAllScripts()
{
    if (!m_impl || !m_impl->L)
        return;
    for (auto &[id, ref] : m_impl->envRefs)
        luaL_unref(m_impl->L, LUA_REGISTRYINDEX, ref);
    m_impl->envRefs.clear();
    m_impl->wasReady.clear();
    m_impl->scriptMtimes.clear();
    clearTimers(m_impl->L); // 未触发的 delay 回调一并回收（防跨会话残留）
}
