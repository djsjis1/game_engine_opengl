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

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <string>
#include <map>
#include <thread>

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
            lua_pop(L, 1);
            return false;
        }
        if (sc.filePath[0])
        {
            CachedChunk cc;
            lua_dump(L, [](lua_State *, const void *p, size_t sz, void *ud) -> int
                     { ((CachedChunk*)ud)->data.append((const char*)p,sz); return 0; }, &cc, 0);
            lua_pop(L, 1);
            struct stat st;
            stat(sc.filePath, &st);
            cc.mtime = st.st_mtime;
            g_compiledChunks[key] = std::move(cc);
            if (luaL_loadstring(L, src.c_str()) != LUA_OK)
            {
                lua_pop(L, 1);
                return false;
            }
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
        lua_pop(L, 1);
        return false;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, funcRef);
    return true;
}

/// 调用沙盒环境中的生命周期函数（ready / update）
static void callLifecycle(lua_State *L, int envRef, const char *name, float dt = 0)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 2);
        return;
    }
    if (dt > 0)
        lua_pushnumber(L, dt);
    if (lua_pcall(L, dt > 0 ? 1 : 0, 0, 0) != LUA_OK)
        lua_pop(L, 1);
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
};

ScriptSystem::~ScriptSystem() = default;

void ScriptSystem::init(flecs::world &world)
{
    m_impl = new Impl;
    m_impl->L = luaL_newstate();
    luaL_openlibs(m_impl->L);
    m_impl->world = &world; // reloadScript 需要遍历实体

    g_mainThreadId = std::this_thread::get_id();

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
        if (m_impl->L)
            lua_close(m_impl->L);
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

    world.each([&](flecs::entity e, TransformComponent &tf, ScriptComponent &sc)
               {
        auto it = m_impl->envRefs.find(e.raw_id());
        if (it == m_impl->envRefs.end()) {
            int envRef;
            if (!initEntity(L, sc, envRef)) return;
            m_impl->envRefs[e.raw_id()] = envRef;
            m_impl->wasReady[e.raw_id()]  = false;
            if (sc.filePath[0]) {
                struct stat st;
                if (stat(sc.filePath, &st) == 0)
                    m_impl->scriptMtimes[e.raw_id()] = st.st_mtime;
            }
            it = m_impl->envRefs.find(e.raw_id());
        }
        int envRef = it->second;

        if (sc.filePath[0])
        {
            struct stat st;
            if (stat(sc.filePath, &st) == 0)
            {
                auto mtimeIt = m_impl->scriptMtimes.find(e.raw_id());
                if (mtimeIt != m_impl->scriptMtimes.end() && mtimeIt->second != st.st_mtime)
                {
                    int oldEnvRef = envRef;
                    int newEnvRef;
                    if (initEntity(L, sc, newEnvRef))
                    {
                        luaL_unref(L, LUA_REGISTRYINDEX, oldEnvRef);
                        m_impl->envRefs[e.raw_id()]    = newEnvRef;
                        m_impl->wasReady[e.raw_id()]    = false;
                        m_impl->scriptMtimes[e.raw_id()] = st.st_mtime;
                        envRef = newEnvRef;
                    }
                    else
                    {
                        m_impl->scriptMtimes[e.raw_id()] = st.st_mtime;
                        return;
                    }
                }
            }
        }

        LuaCtx ctx = { &tf, &world, e.raw_id() };
        lua_pushstring(L, "__active_ctx");
        lua_pushlightuserdata(L, &ctx);
        lua_settable(L, LUA_REGISTRYINDEX);

        if (!m_impl->wasReady[e.raw_id()]) { callLifecycle(L, envRef, "ready"); m_impl->wasReady[e.raw_id()] = true; }
        callLifecycle(L, envRef, "update", dt); });
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
