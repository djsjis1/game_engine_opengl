// LuaTestApi.cpp - Engine headless test Lua API
// Only compiled into engine_tests target. Decoupled from main engine.

#include <cstdio>
#include <cstring>
#include <string>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#pragma push_macro("emit")
#undef emit
#pragma push_macro("signals")
#undef signals
#include <flecs.h>
#pragma pop_macro("signals")
#pragma pop_macro("emit")

#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Systems/PhysicsSystem.h"
#include "Core/Input.h"

// Bridge functions from engine_test_main.cpp
extern void headlessRunFrames(int count, float dt);
extern flecs::world *headlessGetWorld();

// Test statistics
static int g_passCount = 0;
static int g_failCount = 0;

static int api_testAssert(lua_State *L)
{
    bool cond = lua_toboolean(L, 1) != 0;
    const char *msg = luaL_optstring(L, 2, "(unnamed)");
    if (cond)
    {
        printf("  [PASS] %s\n", msg);
        g_passCount++;
    }
    else
    {
        printf("  [FAIL] %s\n", msg);
        g_failCount++;
    }
    return 0;
}

static int api_testAssertNear(lua_State *L)
{
    float actual = (float)luaL_checknumber(L, 1);
    float expected = (float)luaL_checknumber(L, 2);
    float tol = (float)luaL_optnumber(L, 3, 0.01);
    const char *msg = luaL_optstring(L, 4, "(unnamed)");
    float diff = actual - expected;
    if (diff < 0)
        diff = -diff;
    if (diff <= tol)
    {
        printf("  [PASS] %s (%.4f ~ %.4f)\n", msg, actual, expected);
        g_passCount++;
    }
    else
    {
        printf("  [FAIL] %s (got %.4f, want %.4f +/-% .4f)\n", msg, actual, expected, tol);
        g_failCount++;
    }
    return 0;
}

static int api_testRunFrames(lua_State *L)
{
    int n = (int)luaL_checkinteger(L, 1);
    headlessRunFrames(n, 1.0f / 60.0f);
    return 0;
}

static int api_testSimulateKey(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int keyCode = 0;
    if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z')
        keyCode = name[0];
    else if (strcmp(name, "SPACE") == 0)
        keyCode = 0x20;
    else if (strcmp(name, "SHIFT") == 0)
        keyCode = 0x01000020;
    if (keyCode == 0)
    {
        printf("  [WARN] unknown key: %s\n", name);
        return 0;
    }
    // Just press+release without running frames (avoids script re-entry)
    Input::instance().onKey(keyCode, true);
    Input::instance().onKey(keyCode, false);
    return 0;
}

// test_press_key(name) - press and HOLD (call test_release_key later)
static int api_testPressKey(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int keyCode = 0;
    if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z')
        keyCode = name[0];
    else if (strcmp(name, "SPACE") == 0)
        keyCode = 0x20;
    else if (strcmp(name, "SHIFT") == 0)
        keyCode = 0x01000020;
    if (keyCode)
        Input::instance().onKey(keyCode, true);
    return 0;
}

// test_release_key(name) - release a held key
static int api_testReleaseKey(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int keyCode = 0;
    if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z')
        keyCode = name[0];
    else if (strcmp(name, "SPACE") == 0)
        keyCode = 0x20;
    else if (strcmp(name, "SHIFT") == 0)
        keyCode = 0x01000020;
    if (keyCode)
        Input::instance().onKey(keyCode, false);
    return 0;
}

static int api_testGetPosition(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    flecs::world *w = headlessGetWorld();
    float x = 0, y = 0, z = 0;
    if (w)
    {
        w->each([&](flecs::entity e, const TagComponent &tag, const TransformComponent &tf)
                {
            if (strcmp(tag.name, name) == 0) { x = tf.position.x; y = tf.position.y; z = tf.position.z; } });
    }
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

static int api_testGetVelocity(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    flecs::world *w = headlessGetWorld();
    float vx = 0, vy = 0, vz = 0;
    if (w)
    {
        w->each([&](flecs::entity e, const TagComponent &tag, const RigidbodyComponent &rb)
                {
            if (strcmp(tag.name, name) == 0) physicsGetVelocity(e.raw_id(), vx, vy, vz); });
    }
    lua_pushnumber(L, vx);
    lua_pushnumber(L, vy);
    lua_pushnumber(L, vz);
    return 3;
}

static int api_testSection(lua_State *L)
{
    printf("\n--- %s ---\n", luaL_checkstring(L, 1));
    return 0;
}

static int api_testSummary(lua_State *L)
{
    printf("\n============================\n");
    printf("  PASS: %d  FAIL: %d  TOTAL: %d\n", g_passCount, g_failCount, g_passCount + g_failCount);
    printf("============================\n");
    printf(g_failCount > 0 ? "  *** TESTS FAILED ***\n" : "  ALL TESTS PASSED\n");
    return 0;
}

static int api_testFailCount(lua_State *L)
{
    lua_pushinteger(L, g_failCount);
    return 1;
}

static const luaL_Reg testLib[] = {
    {"test_assert", api_testAssert},
    {"test_assert_near", api_testAssertNear},
    {"test_run_frames", api_testRunFrames},
    {"test_simulate_key", api_testSimulateKey},
    {"test_press_key", api_testPressKey},
    {"test_release_key", api_testReleaseKey},
    {"test_get_position", api_testGetPosition},
    {"test_get_velocity", api_testGetVelocity},
    {"test_section", api_testSection},
    {"test_summary", api_testSummary},
    {"test_fail_count", api_testFailCount},
    {nullptr, nullptr}};
void registerTestApis(lua_State *L)
{
    for (const luaL_Reg *fn = testLib; fn->name; ++fn)
    {
        lua_pushcfunction(L, fn->func);
        lua_setglobal(L, fn->name);
    }
}
