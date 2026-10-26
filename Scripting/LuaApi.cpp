// ============================================================
// LuaApi.cpp —— 脚本 API 框架层（1 全局 lua_State + per-entity table 沙盒）
//
// 【本文件职责】只放"框架"，不放具体 API：
//   上下文传递（getCtx/__active_ctx）/ 多线程信号队列 /
//   实体销毁信号清理 / injectApi 注册 / scriptApiList 主表
// 具体 API 实现按域拆分（拆分地图见 LuaApiInternal.h）：
//   LuaApiEntity.cpp / LuaApiInput.cpp / LuaApiGameplay.cpp /
//   LuaApiSignalAnim.cpp / LuaAsync.cpp
//
// ==================== 整体架构 ====================
//
//   【核心理念】一个 Lua VM，多个沙盒环境
//
//   不是每个实体一个 lua_State（那会 ~40KB/实体，100 个实体就 4MB），
//   而是全局一个 lua_State，每个实体有自己独立的 table 做"环境"。
//
//   C++ 每帧                          Lua VM 内部
//   ──────────                       ─────────────
//   ScriptSystem::update()           ╔══════════════════════╗
//     │                              ║ 全局 lua_State       ║
//     │  遍历每个有 ScriptComponent  ║                      ║
//     │  的实体：                    ║  注册表 (REGISTRY)   ║
//     │                              ║  ├── envRef_42      ║ ← 实体 42 的沙盒
//     │  1. 设置 __active_ctx ──────→║  │   ├── get_position║ ← 注入的 API
//     │     (告诉 Lua"现在在执行     ║  │   └── delta()     ║
//     │      实体 42 的脚本")        ║  │                   ║
//     │  2. 调 ready() 或 update()─→ ║  │   脚本里写:       ║
//     │                              ║  │   function update(dt)
//     │  3. API 读取 __active_ctx ←──║  │     set_position(...)
//     │     拿到 Transform* 指针     ║  │   end             ║
//     │     直接修改 C++ 内存!       ║  └───────────────────║
//     │     (零拷贝)                 ╚══════════════════════╝
//
// ==================== 上下文传递 ====================
//
//   问题：Lua 函数被调用时，怎么知道"当前是哪个实体在执行"？
//   方案：ScriptSystem::update() 调 Lua 前把当前实体的
//         TransformComponent*/world*/id 放进注册表 __active_ctx，
//         每个 API 通过 getCtx(L) 取出（脚本不用传 self，写起来简洁）
// ============================================================
#include "LuaApiInternal.h"
#include "LuaAsync.h"          // api_asyncRun（主表引用）
#include "ScriptApiRegistry.h" // API 主表结构体 + scriptApiList() 声明
#include "Core/EngineLog.h"    // engineLogError（print 输出到输出面板）
#include "Components/TransformComponent.h"
#include "Components/SignalComponent.h"
#include <cstdio>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ============================================================
// Lua 编译缓存（与 ScriptEngine.cpp 共享）
// ============================================================
std::map<std::string, CachedChunk> g_compiledChunks;

// ============================================================
// 多线程信号队列 —— 定义已迁到 Core/SignalQueue.cpp（ADR-002）
// 本文件只保留消费侧（processDeferredSignals）和 Lua 相关的
// enqueueSignal（要从 Lua 栈取参，只能留在脚本层）。
// ============================================================
std::thread::id g_mainThreadId;
ScriptApiHook g_extraApiHook = nullptr; // 测试钩子（主程序永远为 null）
// 主 Lua VM 指针（injectApi 时记录）：供 cleanupEntitySignals 在实体
// 销毁时 luaL_unref 回调——observer 回调拿不到 lua_State，用这个全局
static lua_State *g_apiLuaState = nullptr;

// VM 关闭前置空：防 lua_close 后实体销毁触发 cleanupEntitySignals
// 对已关闭的 state 操作（悬空指针崩溃）
void invalidateApiLuaState() { g_apiLuaState = nullptr; }

/// 工作线程调用：emit 入队（线程安全；主线程每帧消费）
void enqueueSignal(flecs::entity_t emitter, const char *name, int nargs, lua_State *L)
{
    DeferredSignal ds;
    ds.emitterId = emitter;
    ds.signalName = name;
    // 从 Lua 栈取出额外参数（跳过第 1 个：信号名）
    for (int i = 2; i <= nargs + 1; ++i)
        ds.args.push_back((double)lua_tonumber(L, i));
    pushDeferredSignal(std::move(ds)); // 统一入口（内部加锁，Core 层）
}

// 实体销毁时清理其信号连接。由 Engine 的 OnRemove observer 调用。
// 注意：observer 在 OnRemove 触发时，死亡实体的 SignalComponent 仍可读
// （尚未真正析构），所以能拿到它的 signalMap/listensTo 做清理。
// 不清会泄漏 Lua 注册表引用 + 源实体 emit 调到已死监听者的孤儿回调。
void cleanupEntitySignals(flecs::world world, flecs::entity_t deadId)
{
    lua_State *L = g_apiLuaState;
    if (!L)
        return; // 脚本系统未初始化（如纯编辑器无 VM）——无 ref 可清
    auto compId = world.component<SignalComponent>().raw_id();
    auto *dead = (SignalComponent *)ecs_get_id(world.c_ptr(), deadId, compId);
    if (!dead)
        return;

    // ① 释放"别人监听我"的回调：我 signalMap 里的所有 ref 都是别的实体
    //   注册进来的（connect_to 到我），我死了这些回调再没意义 → 全 unref
    for (auto &[sigName, conns] : dead->signalMap)
        for (auto &conn : conns)
            luaL_unref(L, LUA_REGISTRYINDEX, conn.callbackRef);

    // ② 我作为监听者去各源实体清理（listensTo 反向索引正是为此设计）：
    //   我 connect_to 了谁 → 去它的 signalMap 删掉 listenerId==我 的连接
    //   并 unref，否则源实体 emit 时会调用指向已死监听者的孤儿回调
    for (auto &[srcId, sigNames] : dead->listensTo)
    {
        auto *src = (SignalComponent *)ecs_get_mut_id(world.c_ptr(), srcId, compId);
        if (!src)
            continue; // 源实体也没了，它的组件已随之清理
        for (const auto &sigName : sigNames)
        {
            auto it = src->signalMap.find(sigName);
            if (it == src->signalMap.end())
                continue;
            auto &vec = it->second;
            for (auto cit = vec.begin(); cit != vec.end();)
            {
                if (cit->listenerId == deadId)
                {
                    luaL_unref(L, LUA_REGISTRYINDEX, cit->callbackRef);
                    cit = vec.erase(cit);
                }
                else
                    ++cit;
            }
        }
    }
}

/// 主线程调用：消费全部排队信号
/// 必须在持有 lua_State 的线程上调用
void processDeferredSignals(lua_State *L, flecs::world &world)
{
    for (auto &ds : takeDeferredSignals())
    {
        flecs::entity e = world.entity(ds.emitterId);
        if (!e)
            continue; // 实体已删除，跳过

        auto compId = world.component<SignalComponent>().raw_id();
        auto *sig = (SignalComponent *)ecs_get_id(world.c_ptr(), ds.emitterId, compId);
        if (!sig)
            continue;

        auto it = sig->signalMap.find(ds.signalName);
        if (it == sig->signalMap.end())
            continue;

        // 为发信号的实体建立上下文，让回调里的 get_position() 等 API 正确作用
        // （不建会导致 __active_ctx 指向上帧已销毁的栈变量 → 悬空指针）
        auto *tf = (TransformComponent *)ecs_get_mut_id(
            world.c_ptr(), ds.emitterId,
            world.component<TransformComponent>().raw_id());
        LuaCtx ctx = {tf, &world, ds.emitterId};
        lua_pushstring(L, "__active_ctx");
        lua_pushlightuserdata(L, &ctx);
        lua_settable(L, LUA_REGISTRYINDEX);

        for (auto &conn : it->second)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, conn.callbackRef);
            if (!lua_isfunction(L, -1))
            {
                lua_pop(L, 1);
                continue;
            }
            for (size_t i = 0; i < ds.args.size(); ++i)
                lua_pushnumber(L, ds.args[i]);
            if (lua_pcall(L, (int)ds.args.size(), 0, 0) != LUA_OK)
                lua_pop(L, 1);
        }
    }

    // 清空上下文：本函数用的 ctx 是栈变量，返回后就销毁了，
    // 不清会成为悬空指针，后续 getCtx 会读到它
    lua_pushstring(L, "__active_ctx");
    lua_pushlightuserdata(L, nullptr);
    lua_settable(L, LUA_REGISTRYINDEX);
}

/// 从 Lua 注册表取回当前实体的上下文
LuaCtx *getCtx(lua_State *L)
{
    lua_pushstring(L, "__active_ctx");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto *c = (LuaCtx *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return c;
}

// ============================================================
// 定时器（对标 Godot SceneTree.create_timer）
// delay(秒, 回调) —— 到点主线程调回调（一次性）。
// 回调 ref 三路径闭环：正常触发 unref / clearTimers unref /
// （不绑实体，无实体死亡路径）。之前"3 秒后重开"借 tween 当定时器
// 的野路子从此转正。
// ============================================================
namespace
{
    struct PendingTimer
    {
        float remaining = 0.0f;
        int cbRef = LUA_NOREF;
    };
    std::vector<PendingTimer> g_timers; // 主线程独占
} // namespace

/// Lua API：delay(seconds, callback)
int api_delay(lua_State *L)
{
    float sec = (float)luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    PendingTimer t;
    t.remaining = sec > 0 ? sec : 0;
    t.cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    g_timers.push_back(t);
    return 0;
}

/// 每帧推进定时器（ScriptSystem::update 调用；到点调回调并 unref）
void processTimers(lua_State *L, float dt)
{
    // 先收集到点的（回调里可能又 delay() 往列表加——不能边遍历边调）
    std::vector<int> fire;
    for (auto it = g_timers.begin(); it != g_timers.end();)
    {
        it->remaining -= dt;
        if (it->remaining <= 0)
        {
            fire.push_back(it->cbRef);
            it = g_timers.erase(it);
        }
        else
            ++it;
    }
    for (int ref : fire)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (lua_isfunction(L, -1))
        {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
                lua_pop(L, 1);
        }
        else
            lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, ref); // 一次性：用完即收
    }
}

/// 退出 Play 清残留（未触发的回调统一 unref，防注册表泄漏）
void clearTimers(lua_State *L)
{
    for (auto &t : g_timers)
        luaL_unref(L, LUA_REGISTRYINDEX, t.cbRef);
    g_timers.clear();
}

// ============================================================
// injectApi —— 把 C++ 函数注入到 Lua 沙盒 table 中
//
// 每个脚本实体的沙盒 table 创建时调用；具体 API 列表见下方主表。
// Lua 端直接调 set_position(0,10,0)（注入到沙盒环境，不需要 self:）
// ============================================================
/// 自定义 print：输出到编辑器输出面板（通过 engineLogError 通道）
static int api_print(lua_State *L)
{
    int n = lua_gettop(L);
    std::string msg;
    for (int i = 1; i <= n; ++i)
    {
        if (i > 1)
            msg += "\t";
        if (lua_isstring(L, i))
            msg += lua_tostring(L, i);
        else if (lua_isnil(L, i))
            msg += "nil";
        else if (lua_isboolean(L, i))
            msg += lua_toboolean(L, i) ? "true" : "false";
        else
            msg += luaL_typename(L, i);
    }
    engineLogError(msg);
    return 0;
}

void injectApi(lua_State *L)
{
    g_apiLuaState = L; // 记录主 VM，供实体销毁时清理信号连接（cleanupEntitySignals）
    // 遍历主表注册：对每个有函数指针的条目，压栈 + 设为字段
    // 代替以前 40 行手写 lua_pushcfunction/lua_setfield（一处登记自动同步）
    for (const auto &e : scriptApiList())
    {
        if (!e.func)
            continue; // 非注入型（如 Lua 内置 print）只入元数据不注册
        lua_pushcfunction(L, e.func);
        lua_setfield(L, -2, e.name);
    }
}

// ============================================================
// kApiTable —— 引擎 Lua API 的单一事实来源（函数指针 + 元数据）
//
// 加一个新 API 只需：①在对应域文件写 api_xxx 实现（见 LuaApiInternal.h
// 拆分地图）②LuaApiInternal.h 加声明 ③在本表加一行。
// injectApi 注册、engine_api.lua 存根、补全词库、高亮词表四处自动同步。
// func 填 nullptr 表示"只有元数据不注册"（如 Lua 内置 print）。
// ============================================================
const std::vector<ScriptApiEntry> &scriptApiList()
{
    static const std::vector<ScriptApiEntry> table = {
        // ---- 基础 ----
        {api_print, "print", "...", "", "打印到输出面板"},
        {api_delta, "delta", "", "number", "本帧时间增量（秒）"},
        {api_isKeyPressed, "is_key_pressed", "key", "boolean", "按键是否按住（持续），支持 \"W\"/\"space\"/\"left\"/\"shift\" 等"},
        {api_isKeyJustPressed, "is_key_just_pressed", "key", "boolean", "按键本帧刚按下（边沿，跳跃/开枪用）"},
        {api_isKeyJustReleased, "is_key_just_released", "key", "boolean", "按键本帧刚松开（边沿）"},
        {api_isActionPressed, "is_action_pressed", "action", "boolean", "动作是否按住（如 \"jump\"，键位可配）"},
        {api_isActionJustPressed, "is_action_just_pressed", "action", "boolean", "动作本帧刚触发（边沿）"},
        // ---- 鼠标 ----
        {api_isMousePressed, "is_mouse_pressed", "btn", "boolean", "鼠标键是否按住（\"left\"/\"middle\"/\"right\"）"},
        {api_isMouseJustPressed, "is_mouse_just_pressed", "btn", "boolean", "鼠标键本帧刚按下（边沿，点击/射击用）"},
        {api_isMouseJustReleased, "is_mouse_just_released", "btn", "boolean", "鼠标键本帧刚松开（边沿，拖拽结束用）"},
        {api_getMousePosition, "get_mouse_position", "", "number x, number y", "鼠标视口像素坐标（配合拾取）"},
        {api_getMouseDelta, "get_mouse_delta", "", "number dx, number dy", "本帧鼠标相对位移（FPS 视角用）"},
        {api_getMouseWheel, "get_mouse_wheel", "", "number", "本帧滚轮格数（上正下负，缩放/切武器用）"},
        {api_setMouseMode, "set_mouse_mode", "mode", "", "光标模式：\"captured\"锁中心(FPS) / \"visible\"正常"},
        {api_getNode, "get_node", "name", "number", "按名字查实体 ID"},
        {api_createEntity, "create_entity", "name", "number", "创建实体，返回其 ID"},
        // ---- 建造 API（纯代码造世界：bootstrap 脚本 ready() 里搭场景）----
        {api_addMesh, "add_mesh", "id, type", "", "加可见网格（\"cube\"），不加不可见"},
        {api_addMaterial, "add_material", "id, r, g, b, metallic, roughness", "", "PBR 材质：基础色 + 金属度(默认0) + 粗糙度(默认0.5)"},
        {api_addTexture, "add_texture", "id, path", "", "设 albedo 贴图（相对路径如 \"textures/wood.png\"）"},
        {api_addRigidbody, "add_rigidbody", "id, motion", "", "加刚体（0=静态 1=运动学 2=动态）"},
        {api_addCollider, "add_collider", "id, hx, hy, hz", "", "加盒碰撞体（半尺寸，默认0.5）"},
        {api_addSphereCollider, "add_sphere_collider", "id, radius", "", "加球形碰撞体（半径默认0.5）"},
        {api_addCapsuleCollider, "add_capsule_collider", "id, radius, height", "", "加胶囊碰撞体（总高默认2.0）"},
        {api_addScript, "add_script", "id, path", "", "挂行为脚本（相对路径如 \"scripts/x.lua\"）"},
        {api_addCamera, "add_camera", "id, px, py, pz, yaw, pitch", "", "加游戏相机（active）"},
        {api_setCamera, "set_camera", "px, py, pz, yaw, pitch", "", "每帧驱动游戏相机（第一人称/跟随视角）"},
        {api_setFog, "set_fog", "r, g, b, density", "", "动态雾（density 0=关，走进雾区/山洞用）"},
        {api_setExposure, "set_exposure", "v", "", "动态曝光（昼夜明暗，1=正常）"},
        {api_tween, "tween", "id, target, x, y, z, duration, easing, done_signal", "",
         "补间动画：target=\"position/rotation/scale\"，easing=\"linear/ease_in/ease_out/ease_in_out/bounce/elastic\"，完成可发信号"},
        // ---- 寻路（NavGrid）----
        {api_navBake, "nav_bake", "", "number", "烘焙导航网格（扫静态碰撞体为障碍），进关卡时调一次"},
        {api_navFindPath, "nav_find_path", "fx, fz, tx, tz", "table|nil",
         "A* 寻路：返回 {{x=..,z=..},...} 路点序列，不可达返回 nil"},
        {api_navIsBlocked, "nav_is_blocked", "x, z", "boolean", "该位置是否是障碍"},
        // ---- 自检/统计（AI Agent 排障接口）----
        {api_engineStats, "engine_stats", "", "string", "运行时状态快照：DrawCall/uniform/实体数/断言结果"},
        {api_engineSelfCheck, "engine_self_check", "", "string", "跑全部自检项，返回 PASS/FAIL 摘要"},
        {api_delay, "delay", "seconds, callback", "", "定时器：秒数后调回调（一次性，Godot create_timer 同款）"},
        {api_addRotate, "add_rotate", "id, sx, sy, sz", "", "自转装饰（度/秒）"},
        {api_addLight, "add_light", "id, r, g, b, intensity", "", "加方向光（场景光照由光源实体驱动）"},
        {api_addParticles, "add_particles", "id, preset", "", "加粒子发射器（\"fire\"/\"smoke\"/\"sparkle\"，参数编辑器可调）"},
        {api_particlesEmit, "particles_emit", "id, on", "", "开/关粒子发射（已有粒子自然飘完）"},
        {api_setNodeScale, "set_node_scale", "id, x, y, z", "", "缩放任意实体"},
        {api_destroyEntity, "destroy_entity", "id", "", "删除实体（资源自动回收）"},
        // ---- 当前实体 Transform ----
        {api_getPosition, "get_position", "", "number x, number y, number z", "获取实体世界坐标"},
        {api_setPosition, "set_position", "x, y, z", "", "设置实体世界坐标"},
        {api_getRotation, "get_rotation", "", "number x, number y, number z", "获取实体旋转（欧拉角度）"},
        {api_setRotation, "set_rotation", "x, y, z", "", "设置实体旋转（欧拉角度）"},
        {api_getScale, "get_scale", "", "number x, number y, number z", "获取缩放"},
        {api_setScale, "set_scale", "x, y, z", "", "设置缩放"},
        {api_setVisible, "set_visible", "visible", "", "显示/隐藏实体"},
        {api_isVisible, "is_visible", "", "boolean", "是否可见"},
        // ---- 跨实体操作 ----
        {api_getNodePosition, "get_node_position", "id", "number x, number y, number z", "获取指定实体位置"},
        {api_getNodeRotation, "get_node_rotation", "id", "number rx, number ry, number rz", "获取指定实体旋转（欧拉角度）"},
        {api_setNodePosition, "set_node_position", "id, x, y, z", "", "设置指定实体位置"},
        // ---- 物理（Jolt）----
        {api_applyImpulse, "apply_impulse", "x, y, z", "", "施加冲量（瞬间推力，如跳跃）"},
        {api_setVelocity, "set_velocity", "x, y, z", "", "直接设置刚体线速度"},
        {api_getVelocity, "get_velocity", "", "number vx, number vy, number vz",
         "读取刚体速度（移动时保留 y：set_velocity(mx, vy, mz)）"},
        {api_moveSlide, "move_slide", "vx, vz", "", "角色水平移动（竖直速度自动保留，CharacterBody3D 同款）"},
        {api_isOnFloor, "is_on_floor", "", "boolean", "是否站在地面（跳跃判定一行搞定）"},
        // ---- 玩家存档（exe旁 saves/，存什么由脚本定）----
        {api_saveSet, "save_set", "key, value", "", "写存档草稿（number/string/bool）"},
        {api_saveGet, "save_get", "key, default", "any", "读存档草稿（无此键返回 default）"},
        {api_saveWrite, "save_write", "slot", "boolean", "草稿落盘到槽位（默认1），玩家按保存时调"},
        {api_saveLoad, "save_load", "slot", "boolean", "读档到草稿（默认槽1），继续游戏时调"},
        {api_saveExists, "save_exists", "slot", "boolean", "存档是否存在（主菜单'继续'判断）"},
        // ---- 多场景 ----
        {api_changeScene, "change_scene", "path", "", "切换场景（帧末生效），如 change_scene(\"scenes/level2.json\")"},
        {api_getScene, "get_scene", "", "string", "当前场景相对路径（判断自己在哪关）"},
        // ---- 音频（miniaudio）----
        {api_playSound, "play_sound", "path, volume", "boolean", "播一次性音效（相对路径如 \"sounds/jump.wav\"）"},
        {api_playMusic, "play_music", "path, volume", "boolean", "播/切背景音乐（循环单例）"},
        {api_stopMusic, "stop_music", "", "", "停止背景音乐"},
        {api_setMasterVolume, "set_master_volume", "v", "", "主音量 0~1（影响全部声音）"},
        {api_setBusVolume, "set_bus_volume", "bus, v", "", "总线音量：\"music\"/\"sfx\"/\"master\"（设置界面滑块用）"},
        {api_raycast, "raycast", "ox, oy, oz, dx, dy, dz, maxDist", "table|nil",
         "物理射线，命中返回 {entity,x,y,z,dist}，未命中 nil"},
        // ---- 父子层级 ----
        {api_setParent, "set_parent", "child, parent", "", "设置父子关系（child_of）"},
        {api_getParent, "get_parent", "id", "number", "获取父实体 ID（无父返回 0）"},
        // ---- 信号 ----
        {api_connect, "connect", "signal, callback", "", "监听本实体信号"},
        {api_emit, "emit", "signal, ...", "", "发射信号（可带参数）"},
        {api_connectTo, "connect_to", "id, signal, callback", "", "监听指定实体的信号"},
        {api_disconnect, "disconnect", "signal, ref", "", "断开信号监听"},
        // ---- 异步（WorkerThreadPool）----
        {api_asyncRun, "async", "fn, callback", "", "异步执行计算，完成后回调"},
        // ---- 脚本变量（@export 风格）----
        {api_export, "export", "name, default", "", "声明可导出变量（编辑器可调）"},
        {api_getProp, "get_prop", "name", "any", "读取脚本变量"},
        {api_setProp, "set_prop", "name, value", "", "设置脚本变量"},
        // ---- 动画播放（AnimationPlayer 风格）----
        {api_animPlay, "anim_play", "clip", "", "从头播放指定动画"},
        {api_animCrossfade, "anim_crossfade", "clip, duration", "", "带过渡切换到新动画（默认0.3s，走→跑无缝混合）"},
        {api_animStop, "anim_stop", "", "", "停止播放（保持当前姿态）"},
        {api_animIsPlaying, "anim_is_playing", "", "boolean", "是否正在播放"},
        {api_animSetSpeed, "anim_set_speed", "speed", "", "播放速度倍率（1=正常 2=两倍 负=倒放）"},
        {api_animSetLoop, "anim_set_loop", "loop", "", "循环开关（true/false）"},
        // ---- 动画设计（脚本程序化创建）----
        {api_animSetFile, "anim_set_file", "path", "", "绑定动画文件（无组件时自动挂）"},
        {api_animCreate, "anim_create", "clip, duration", "", "新建动画（同名覆盖）"},
        {api_animAddKey, "anim_add_key", "clip, track, t, x, y, z, easing", "",
         "打关键帧。track 如 \"position\" 或 \"组件名/字段名\"，easing 可选"},
        {api_animAddEvent, "anim_add_event", "clip, t, signal", "", "事件轨道：播到 t 时发信号"},
        {api_animSave, "anim_save", "", "boolean", "把动画库写回 .anim 文件"},
        // ---- 动画状态机（AnimationTree StateMachine 风格）----
        {api_animFsmAdd, "anim_fsm_add", "id, from, to, param, op, threshold, blend", "",
         "加转换规则：from 状态且 param >/< threshold → crossfade 到 to"},
        {api_animFsmStart, "anim_fsm_start", "id, state", "", "设初始状态并启动状态机"},
        {api_animFsmParam, "anim_fsm_param", "id, param, value", "", "喂参数（切换自动发生）"},
        {api_animFsmState, "anim_fsm_state", "id", "string", "查询当前状态名"},
    };
    return table;
}

// ============================================================
// ScriptSystem 实现在 ScriptEngine.cpp 中（按职责拆分）
// ============================================================
