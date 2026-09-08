/**
 * lua_practice.cpp —— Lua C API 练习场
 *
 * 本文件包含 6 个渐进式练习，从上到下依次运行。
 * 每个练习演示 Lua 与 C++ 交互的一个核心概念。
 *
 * 学习顺序:
 *   练习 1 → 练习 2 → 练习 3 → 练习 4 → 练习 5 → 练习 6
 *   每个练习独立输出一段结果，方便观察。
 */

#include <cstdio>
#include <cstring>
#include <cstdlib> // system() for chcp

// Lua 头文件（C 库，用 extern "C" 包起来）
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// ============================================================
// 辅助工具
// ============================================================

/// 打印栈内容（调试用）
static void printStack(lua_State *L, const char *label)
{
    int top = lua_gettop(L);
    printf("  [栈 %s] 深度=%d: ", label, top);
    for (int i = 1; i <= top; ++i)
    {
        int t = lua_type(L, i);
        switch (t)
        {
        case LUA_TNIL:
            printf("nil ");
            break;
        case LUA_TBOOLEAN:
            printf("%s ", lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("%.1f ", lua_tonumber(L, i));
            break;
        case LUA_TSTRING:
            printf("\"%s\" ", lua_tostring(L, i));
            break;
        case LUA_TFUNCTION:
            printf("func ");
            break;
        default:
            printf("? ");
            break;
        }
    }
    printf("\n");
}

// ============================================================
// 练习 1：创建 VM + 栈的 push/pop
// ============================================================
static void exercise1_createAndStack()
{
    printf("=== 练习 1：创建 VM + 栈的 push/pop ===\n");

    lua_State *L = luaL_newstate();
    printf("  VM 创建成功\n");

    lua_pushnumber(L, 3.14);
    lua_pushinteger(L, 42);
    lua_pushstring(L, "Hello Lua");
    lua_pushboolean(L, 1);
    printStack(L, "push 4 个值后");

    printf("  索引 1: %.2f\n", lua_tonumber(L, 1));
    printf("  索引 2: %lld\n", lua_tointeger(L, 2));
    printf("  索引 3: %s\n", lua_tostring(L, 3));
    printf("  索引 -1 (栈顶): %d\n", lua_toboolean(L, -1));

    lua_pop(L, 2);
    printStack(L, "pop 2 个后");

    lua_close(L);
    printf("  VM 已销毁\n\n");
}

// ============================================================
// 练习 2：执行 Lua 代码
// ============================================================
static void exercise2_executeLua()
{
    printf("=== 练习 2：执行 Lua 代码 ===\n");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    const char *code1 = "print('>>> Lua says: Hello from Lua VM!')";
    if (luaL_dostring(L, code1) == LUA_OK)
        printf("  代码 1 执行成功\n");

    const char *code2 = "return 10 + 20 * 3";
    if (luaL_dostring(L, code2) == LUA_OK)
    {
        printf("  代码 2 结果: %.0f\n", lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    const char *badCode = "print(undefined_var + 1)";
    if (luaL_dostring(L, badCode) != LUA_OK)
    {
        printf("  代码 3 错误: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_close(L);
    printf("\n");
}

// ============================================================
// 练习 3：注册 C 函数给 Lua 调用
// ============================================================

static int c_add(lua_State *L)
{
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);
    return 1;
}

static int c_getInfo(lua_State *L)
{
    lua_pushstring(L, "Cube");
    lua_pushnumber(L, 42);
    lua_pushnumber(L, 3.14);
    return 3;
}

static void exercise3_registerCFunction()
{
    printf("=== 练习 3：注册 C 函数给 Lua ===\n");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, c_add);
    lua_setglobal(L, "my_add");
    lua_pushcfunction(L, c_getInfo);
    lua_setglobal(L, "get_info");

    const char *code = R"(
        print(">>> my_add(10, 25) = " .. my_add(10, 25))
        local name, id, val = get_info()
        print(">>> get_info() = " .. name .. ", " .. id .. ", " .. val)
    )";
    luaL_dostring(L, code);

    lua_close(L);
    printf("\n");
}

// ============================================================
// 练习 4：C 端读写 Lua 全局变量
// ============================================================
static void exercise4_readWriteGlobals()
{
    printf("=== 练习 4：C 端读写 Lua 全局变量 ===\n");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_dostring(L, "player_name = 'Hero'");
    luaL_dostring(L, "player_hp = 100");
    luaL_dostring(L, "player_alive = true");

    lua_getglobal(L, "player_name");
    printf("  读取 player_name = %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "player_hp");
    printf("  读取 player_hp   = %.0f\n", lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "player_alive");
    printf("  读取 player_alive = %s\n", lua_toboolean(L, -1) ? "true" : "false");
    lua_pop(L, 1);

    lua_pushnumber(L, 80);
    lua_setglobal(L, "player_hp");
    luaL_dostring(L, "print('>>> Lua 确认: player_hp = ' .. player_hp)");

    lua_close(L);
    printf("\n");
}

// ============================================================
// 练习 5：模拟 __active_ctx 上下文传递
// ============================================================

struct EntityCtx
{
    const char *name;
    float x, y, z;
};

static int ctx_getPosition(lua_State *L)
{
    lua_pushstring(L, "__ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *ctx = (EntityCtx *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    lua_pushnumber(L, ctx->x);
    lua_pushnumber(L, ctx->y);
    lua_pushnumber(L, ctx->z);
    return 3;
}

static int ctx_getName(lua_State *L)
{
    lua_pushstring(L, "__ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *ctx = (EntityCtx *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    lua_pushstring(L, ctx->name);
    return 1;
}

static void exercise5_contextPattern()
{
    printf("=== 练习 5：模拟 __active_ctx 上下文传递 ===\n");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, ctx_getPosition);
    lua_setglobal(L, "get_position");
    lua_pushcfunction(L, ctx_getName);
    lua_setglobal(L, "get_name");

    EntityCtx entityA = {"Cube", 0.0f, 0.0f, 2.0f};
    EntityCtx entityB = {"Sphere", 3.0f, 1.0f, 5.0f};

    printf("  --- 实体 A (%s) 执行 ---\n", entityA.name);
    lua_pushstring(L, "__ctx");
    lua_pushlightuserdata(L, &entityA);
    lua_settable(L, LUA_REGISTRYINDEX);
    luaL_dostring(L, "local x,y,z = get_position(); print('  Lua: name=' .. get_name() .. ' pos=(' .. x .. ', ' .. y .. ', ' .. z .. ')')");

    printf("  --- 实体 B (%s) 执行 ---\n", entityB.name);
    lua_pushstring(L, "__ctx");
    lua_pushlightuserdata(L, &entityB);
    lua_settable(L, LUA_REGISTRYINDEX);
    luaL_dostring(L, "local x,y,z = get_position(); print('  Lua: name=' .. get_name() .. ' pos=(' .. x .. ', ' .. y .. ', ' .. z .. ')')");

    lua_close(L);
    printf("\n");
}

// ============================================================
// 练习 6：迷你脚本系统 —— 每实体独立沙盒
// ============================================================

static lua_State *g_L = nullptr;

struct Sandbox
{
    int envRef;
    float x, y;
};
static Sandbox g_sandboxes[3];
static int g_sandboxCount = 0;

static int sb_move(lua_State *L)
{
    lua_pushstring(L, "__ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *sb = (Sandbox *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    sb->x += (float)luaL_checknumber(L, 1);
    sb->y += (float)luaL_checknumber(L, 2);
    return 0;
}

static int sb_getPos(lua_State *L)
{
    lua_pushstring(L, "__ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *sb = (Sandbox *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    lua_pushnumber(L, sb->x);
    lua_pushnumber(L, sb->y);
    return 2;
}

static void injectAPI(lua_State *L)
{
    lua_pushcfunction(L, sb_move);
    lua_setfield(L, -2, "move");
    lua_pushcfunction(L, sb_getPos);
    lua_setfield(L, -2, "get_position");
}

static void createSandbox(const char *name, float x, float y, const char *script)
{
    printf("  --- 创建实体: %s (%.0f, %.0f) ---\n", name, x, y);

    Sandbox &sb = g_sandboxes[g_sandboxCount];
    sb.x = x;
    sb.y = y;

    lua_newtable(g_L);
    lua_newtable(g_L);
    lua_getglobal(g_L, "_G");
    lua_setfield(g_L, -2, "__index");
    lua_setmetatable(g_L, -2);
    injectAPI(g_L);
    sb.envRef = luaL_ref(g_L, LUA_REGISTRYINDEX);
    g_sandboxCount++;

    lua_pushstring(g_L, "__ctx");
    lua_pushlightuserdata(g_L, &sb);
    lua_settable(g_L, LUA_REGISTRYINDEX);

    if (luaL_loadstring(g_L, script) != LUA_OK)
    {
        printf("  编译错误: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return;
    }
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, sb.envRef);
    lua_setupvalue(g_L, -2, 1);

    if (lua_pcall(g_L, 0, 0, 0) != LUA_OK)
    {
        printf("  执行错误: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
    printf("  沙盒创建完成\n");
}

static void callSandboxFunc(int index, const char *funcName)
{
    Sandbox &sb = g_sandboxes[index];

    // 设置 __ctx 上下文
    lua_pushstring(g_L, "__ctx");
    lua_pushlightuserdata(g_L, &sb);
    lua_settable(g_L, LUA_REGISTRYINDEX);

    // 取出沙盒 table, 再取它的方法
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, sb.envRef); // [sandbox]
    lua_getfield(g_L, -1, funcName);                // [sandbox, sandbox.func]
    if (!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 2);
        return;
    }
    // ★ 把 sandbox table 作为 self 传给函数
    //    Lua 脚本里 self:move(1,0) 需要 self 指向沙盒 table
    lua_pushvalue(g_L, -2);                // [sandbox, func, sandbox(作self)]
    if (lua_pcall(g_L, 1, 0, 0) != LUA_OK) // 1 个参数 = self
    {
        printf("  [%s] 错误: %s\n", funcName, lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
    lua_pop(g_L, 1); // 弹出 sandbox
}

static void exercise6_miniSandbox()
{
    printf("=== 练习 6：迷你脚本系统 —— 沙盒隔离 ===\n");

    g_L = luaL_newstate();
    luaL_openlibs(g_L);

    createSandbox("Runner", 0, 0, R"LUA(
        print("  [Runner] 初始化完成")
        function update()
            self:move(1, 0)
            local x, y = self:get_position()
            print("  [Runner] 位置: (" .. x .. ", " .. y .. ")")
        end
    )LUA");

    createSandbox("Climber", 5, 0, R"LUA(
        print("  [Climber] 初始化完成")
        function update()
            self:move(0, 2)
            local x, y = self:get_position()
            print("  [Climber] 位置: (" .. x .. ", " .. y .. ")")
        end
    )LUA");

    for (int frame = 1; frame <= 3; ++frame)
    {
        printf("\n  === 第 %d 帧 ===\n", frame);
        callSandboxFunc(0, "update");
        callSandboxFunc(1, "update");
    }

    printf("\n  最终位置:\n");
    printf("    Runner:  (%.0f, %.0f)\n", g_sandboxes[0].x, g_sandboxes[0].y);
    printf("    Climber: (%.0f, %.0f)\n", g_sandboxes[1].x, g_sandboxes[1].y);

    lua_close(g_L);
    printf("\n");
}

// ============================================================
// main
// ============================================================

int main()
{
    system("chcp 65001 > nul");

    lua_State *L = luaL_newstate(); // 创建 Lua 状态机
    luaL_openlibs(L);               // 加载标准库
}
