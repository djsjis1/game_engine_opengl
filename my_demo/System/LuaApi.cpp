// ============================================================
// LuaApi.cpp —— Godot 风格 Lua 脚本引擎实现
//
// 安全隔离：Lua C API 全部在一个 .cpp 里，用 push_macro 隔离
// 所有 Lua 宏（LUA_API/LUAI_FUNC/LUALIB_API），确保不和 Qt MOC 冲突。
//
// 架构：
//   ScriptSystem::update()
//     → 遍历 (Transform + Script) 实体
//     → 每个实体独立的 lua_State + 沙盒
//     → 注入所有的 API 函数
//     → 调用 ready() / update(dt)
//     → 分发信号（emit/connect）
// ============================================================

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

#include "ScriptSystem.h"
#include "../Components/ScriptComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/TagComponent.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

struct CachedChunk
{
    std::string data;
    time_t mtime = 0;
};
static std::map<std::string, CachedChunk> g_compiledChunks;

// ---- 信号系统：全局信号表 ----
//   g_signals[entity_id][signal_name] = lua_State* 列表
//   每个 lua_State 里注册了 on_signal(signal_name, ...) 函数
static std::map<flecs::entity_t, std::map<std::string, std::vector<lua_State *>>> g_signals;

// ---- 外部输入注入（由 MainWindow 每帧设置） ----
struct InputState
{
    bool keys[256] = {};
    float mouseDX = 0, mouseDY = 0;
};
static InputState g_input;

// ---- 每帧的 delta time ----
static float g_deltaTime = 0.0f;

extern "C" void scriptSetInput(const bool keys[256], float dx, float dy)
{
    memcpy(g_input.keys, keys, sizeof(g_input.keys));
    g_input.mouseDX = dx;
    g_input.mouseDY = dy;
}
extern "C" void scriptSetDelta(float dt) { g_deltaTime = dt; }

// ---- Lua 上下文（每个实体一个实例） ----
struct LuaCtx
{
    TransformComponent *tf;
    flecs::world *world;
    flecs::entity_t entityId;
};

static LuaCtx *getCtx(lua_State *L)
{
    lua_pushstring(L, "__ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *c = (LuaCtx *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return c;
}

// ============================================================
// ★★★ 实体 API ★★★
// ============================================================

// create_entity("Name") → entity_id
static int api_createEntity(lua_State *L)
{
    auto *ctx = getCtx(L);
    const char *name = luaL_optstring(L, 1, "Entity");
    auto e = ctx->world->entity();
    TagComponent tc{};
    strncpy(tc.name, name, sizeof(tc.name) - 1);
    auto tagId = ctx->world->component<TagComponent>().raw_id();
    e.set_ptr(tagId, sizeof(TagComponent), &tc);
    e.set<TransformComponent>({});
    lua_pushinteger(L, (lua_Integer)e.raw_id());
    return 1;
}

// destroy_entity(id)
static int api_destroyEntity(lua_State *L)
{
    auto *ctx = getCtx(L);
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    ctx->world->entity(eid).destruct();
    return 0;
}

// get_node("Name") → id, found
static int api_getNode(lua_State *L)
{
    auto *ctx = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    ctx->world->each([&](flecs::entity e, const TagComponent &tag)
                     {
        if (strcmp(tag.name, name) == 0) { lua_pushinteger(L, (lua_Integer)e.raw_id()); } });
    return lua_gettop(L) > 1 ? 1 : 0;
}

// ============================================================
// ★★★ Transform API ★★★
// ============================================================

static int api_getPosition(lua_State *L)
{
    auto *c = getCtx(L);
    lua_pushnumber(L, c->tf->position.x);
    lua_pushnumber(L, c->tf->position.y);
    lua_pushnumber(L, c->tf->position.z);
    return 3;
}
static int api_setPosition(lua_State *L)
{
    auto *c = getCtx(L);
    c->tf->position.x = (float)luaL_checknumber(L, 1);
    c->tf->position.y = (float)luaL_checknumber(L, 2);
    c->tf->position.z = (float)luaL_checknumber(L, 3);
    return 0;
}
static int api_getRotation(lua_State *L)
{
    auto *c = getCtx(L);
    lua_pushnumber(L, c->tf->rotation.x);
    lua_pushnumber(L, c->tf->rotation.y);
    lua_pushnumber(L, c->tf->rotation.z);
    return 3;
}
static int api_setRotation(lua_State *L)
{
    auto *c = getCtx(L);
    c->tf->rotation.x = (float)luaL_checknumber(L, 1);
    c->tf->rotation.y = (float)luaL_checknumber(L, 2);
    c->tf->rotation.z = (float)luaL_checknumber(L, 3);
    return 0;
}
static int api_getScale(lua_State *L)
{
    auto *c = getCtx(L);
    lua_pushnumber(L, c->tf->scale.x);
    lua_pushnumber(L, c->tf->scale.y);
    lua_pushnumber(L, c->tf->scale.z);
    return 3;
}
static int api_setScale(lua_State *L)
{
    auto *c = getCtx(L);
    c->tf->scale.x = (float)luaL_checknumber(L, 1);
    c->tf->scale.y = (float)luaL_checknumber(L, 2);
    c->tf->scale.z = (float)luaL_checknumber(L, 3);
    return 0;
}

// get_node_position(id) → x, y, z
static int api_getNodePosition(lua_State *L)
{
    auto *ctx = getCtx(L);
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto *tf = (TransformComponent *)ecs_get_id(ctx->world->c_ptr(), eid,
                                                ctx->world->component<TransformComponent>().raw_id());
    if (!tf)
    {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
        return 3;
    }
    lua_pushnumber(L, tf->position.x);
    lua_pushnumber(L, tf->position.y);
    lua_pushnumber(L, tf->position.z);
    return 3;
}
static int api_setNodePosition(lua_State *L)
{
    auto *ctx = getCtx(L);
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto *tf = (TransformComponent *)ecs_get_mut_id(ctx->world->c_ptr(), eid,
                                                    ctx->world->component<TransformComponent>().raw_id());
    if (!tf)
        return 0;
    tf->position.x = (float)luaL_checknumber(L, 2);
    tf->position.y = (float)luaL_checknumber(L, 3);
    tf->position.z = (float)luaL_checknumber(L, 4);
    return 0;
}

// ============================================================
// ★★★ 信号系统 ★★★
// Godot 风格：connect / emit / connect_to
// ============================================================

// connect("signal_name", callback)
static int api_connect(lua_State *L)
{
    auto *ctx = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // 存储到全局信号表
    g_signals[ctx->entityId][name].push_back(L);
    // 把 ref 存为该函数的上值... 简化：直接存 function 到 registry
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_setglobal(L, (std::string("__sig_") + name).c_str());
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return 0;
}

// emit("signal_name", ...)  — 触发当前实体的信号
static int api_emit(lua_State *L)
{
    auto *ctx = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L) - 1;

    auto itSignals = g_signals.find(ctx->entityId);
    if (itSignals == g_signals.end())
        return 0;

    auto it = itSignals->second.find(name);
    if (it == itSignals->second.end())
        return 0;

    // 调用所有注册的回调
    for (auto *state : it->second)
    {
        // 调用 on_signal(signal_name, ...) 或直接调回调
        lua_getglobal(state, "on_signal");
        if (!lua_isfunction(state, -1))
        {
            lua_pop(state, 1);
            continue;
        }
        lua_pushstring(state, name);
        for (int i = 0; i < nargs; ++i)
            lua_pushvalue(state, i + 2); // 跳过 name
        if (lua_pcall(state, 1 + nargs, 0, 0) != LUA_OK)
            fprintf(stderr, "[Lua] emit: %s\n", lua_tostring(state, -1));
        lua_pop(state, 1);
    }

    // 同时调用当前实体的 on_signal
    lua_getglobal(L, "on_signal");
    if (lua_isfunction(L, -1))
    {
        lua_pushstring(L, name);
        for (int i = 0; i < nargs; ++i)
            lua_pushvalue(L, i + 2);
        if (lua_pcall(L, 1 + nargs, 0, 0) != LUA_OK)
            fprintf(stderr, "[Lua] emit: %s\n", lua_tostring(L, -1));
    }
    lua_pop(L, 1);
    return 0;
}

// connect_to(entity_id, "signal_name", callback) — 连接其他实体的信号
static int api_connectTo(lua_State *L)
{
    auto *ctx = getCtx(L);
    auto targetId = (flecs::entity_t)luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    g_signals[targetId][name].push_back(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_setglobal(L, (std::string("__cb_") + name + "_" + std::to_string(ctx->entityId)).c_str());
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return 0;
}

// ============================================================
// ★★★ 输入 API ★★★
// ============================================================

// is_key_pressed("W") → bool
static int api_isKeyPressed(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    char c = (k && k[0]) ? k[0] : 0;
    if (c >= 'a' && c <= 'z')
        c -= 32; // 小写转大写
    lua_pushboolean(L, g_input.keys[(unsigned char)c]);
    return 1;
}

// get_mouse_delta() → dx, dy
static int api_getMouseDelta(lua_State *L)
{
    lua_pushnumber(L, g_input.mouseDX);
    lua_pushnumber(L, g_input.mouseDY);
    return 2;
}

// ============================================================
// ★★★ 时间 API ★★★
// ============================================================

static int api_delta(lua_State *L)
{
    lua_pushnumber(L, g_deltaTime);
    return 1;
}

// ============================================================
// 注册所有 API 到 Lua 全局
// ============================================================
static void registerAllApi(lua_State *L)
{
    lua_register(L, "create_entity", api_createEntity);
    lua_register(L, "destroy_entity", api_destroyEntity);
    lua_register(L, "get_node", api_getNode);
    lua_register(L, "get_position", api_getPosition);
    lua_register(L, "set_position", api_setPosition);
    lua_register(L, "get_rotation", api_getRotation);
    lua_register(L, "set_rotation", api_setRotation);
    lua_register(L, "get_scale", api_getScale);
    lua_register(L, "set_scale", api_setScale);
    lua_register(L, "get_node_position", api_getNodePosition);
    lua_register(L, "set_node_position", api_setNodePosition);
    lua_register(L, "connect", api_connect);
    lua_register(L, "emit", api_emit);
    lua_register(L, "connect_to", api_connectTo);
    lua_register(L, "is_key_pressed", api_isKeyPressed);
    lua_register(L, "get_mouse_delta", api_getMouseDelta);
    lua_register(L, "delta", api_delta);
}

// ============================================================
// 源码加载（文件优先）
// ============================================================
static std::string loadSource(const ScriptComponent &sc)
{
    if (sc.filePath[0] != '\0')
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

static std::string getCacheKey(const ScriptComponent &sc)
{
    if (sc.filePath[0])
        return std::string(sc.filePath);
    return "inline:" + std::string(sc.source);
}

// ============================================================
// 编译 + 缓存
// ============================================================
static bool compileAndCache(lua_State *L, const ScriptComponent &sc)
{
    std::string key = getCacheKey(sc);

    // 热重载检测：比较文件时间戳
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

    if (!needRecompile)
    {
        auto &cc = g_compiledChunks[key];
        return luaL_loadbuffer(L, cc.data.data(), cc.data.size(), key.c_str()) == LUA_OK;
    }

    // 首次编译或文件已修改：重新编译
    std::string src = loadSource(sc);
    if (src.empty())
        return false;
    if (luaL_loadstring(L, src.c_str()) != LUA_OK)
    {
        fprintf(stderr, "[Lua] compile: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    CachedChunk cc;
    lua_dump(L, [](lua_State *, const void *p, size_t sz, void *ud) -> int
             {
        ((CachedChunk *)ud)->data.append((const char *)p, sz); return 0; }, &cc, 0);
    lua_pop(L, 1);
    if (sc.filePath[0])
    {
        struct stat st;
        stat(sc.filePath, &st);
        cc.mtime = st.st_mtime;
    }
    g_compiledChunks[key] = std::move(cc);

    return luaL_loadstring(L, src.c_str()) == LUA_OK;
}

// ============================================================
// 获取或创建实体的 Lua 状态
// ============================================================
static lua_State *getOrCreateState(flecs::entity e, ScriptComponent &sc,
                                   std::map<flecs::entity_t, void *> &states)
{
    auto it = states.find(e.raw_id());
    if (it != states.end())
        return (lua_State *)it->second;

    lua_State *L = luaL_newstate();
    if (!L)
        return nullptr;
    luaL_openlibs(L);
    registerAllApi(L);

    if (!compileAndCache(L, sc))
    {
        lua_close(L);
        return nullptr;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "[Lua] run: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return nullptr;
    }
    states[e.raw_id()] = L;
    return L;
}

// ============================================================
// 调用生命周期函数
// ============================================================
static void callLifecycle(lua_State *L, const char *name, float dt = 0)
{
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    if (dt > 0)
        lua_pushnumber(L, dt);
    if (lua_pcall(L, dt > 0 ? 1 : 0, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "[Lua] %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

// ============================================================
// ScriptSystem::update()
// ============================================================
void ScriptSystem::update(flecs::world &world, float dt)
{
    if (!m_enabled)
        return;
    scriptSetDelta(dt);

    world.each([&](flecs::entity e, TransformComponent &tf, ScriptComponent &sc)
               {
        lua_State *L = getOrCreateState(e, sc, m_states);
        if (!L) return;

        LuaCtx ctx = { &tf, &world, e.raw_id() };
        lua_pushstring(L, "__ctx");
        lua_pushlightuserdata(L, &ctx);
        lua_settable(L, LUA_REGISTRYINDEX);

        if (!sc.wasReady) { callLifecycle(L, "ready"); sc.wasReady = true; }
        callLifecycle(L, "update", dt); });
}
