// ============================================================
// LuaApiGameplay.cpp — 玩法类脚本 API
//   物理（冲量/速度/射线）/ 玩家存档 / 多场景切换 /
//   音频播放 / 游戏相机驱动 / 环境（雾/曝光）
// 拆分地图见 LuaApiInternal.h；加新玩法 API：实现写这里 + 主表加一行。
// ============================================================
#include "LuaApiInternal.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/AudioSystem.h"
#include "Systems/TweenSystem.h" // tween 补间投递
#include "Components/CameraComponent.h"
#include "Components/EnvironmentComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Core/GamePaths.h"
#include "Core/SaveGame.h"
#include "Core/SceneRouter.h"
#include "Core/NavGrid.h"       // nav_* 寻路 API
#include "Core/TestFramework.h" // 自检/统计（Agent 排障接口）
#include <cstring>
#include <glm/gtc/matrix_transform.hpp> // set_camera 当帧重算 viewMatrix

// ---------------- 引擎自检 / 统计（AI Agent 排障接口） ----------------

/// engine_stats() → string — 运行时状态快照（DrawCall/uniform/实体数/断言结果）
int api_engineStats(lua_State *L)
{
    lua_pushstring(L, EngineTest::statsReport().c_str());
    return 1;
}

/// engine_self_check() → string — 跑全部自检项，返回 "PASS n / FAIL m"
int api_engineSelfCheck(lua_State *L)
{
    std::string summary = EngineTest::runSelfCheck();

    // 世界级检查：带 ECS 上下文时补实体健康检查（悬空/缺组件）
    auto *c = getCtx(L);
    if (c && c->w)
    {
        uint64_t count = 0;
        c->w->each([&](flecs::entity e, const TransformComponent &)
                   { (void)e; ++count; });
        EngineTest::stats().entityCount.store(count, std::memory_order_relaxed);
        summary += " | entities " + std::to_string(count);
    }
    lua_pushstring(L, summary.c_str());
    return 1;
}

// ---------------- 物理（Jolt 驱动） ----------------

/// apply_impulse(x, y, z) — 给当前实体施加冲量（如跳跃）
int api_applyImpulse(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c)
        return 0;
    physicsApplyImpulse(c->id,
                        (float)luaL_checknumber(L, 1),
                        (float)luaL_checknumber(L, 2),
                        (float)luaL_checknumber(L, 3));
    return 0;
}

/// set_velocity(x, y, z) — 直接设置当前实体速度
int api_setVelocity(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c)
        return 0;
    physicsSetVelocity(c->id,
                       (float)luaL_checknumber(L, 1),
                       (float)luaL_checknumber(L, 2),
                       (float)luaL_checknumber(L, 3));
    return 0;
}

/// get_velocity() → vx, vy, vz（无刚体返回 0,0,0）
/// 玩家移动标准写法：改水平速度、保留 y（重力/跳跃不被抹掉）：
///   local vx,vy,vz = get_velocity(); set_velocity(mx, vy, mz)
int api_getVelocity(lua_State *L)
{
    auto *c = getCtx(L);
    float x = 0, y = 0, z = 0;
    if (c)
        physicsGetVelocity(c->id, x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

/// move_slide(vx, vz) — 角色水平移动（竖直速度自动保留，CharacterBody3D 同款）
/// 玩家移动从 3 行样板缩成 1 行：move_slide(mx * speed, mz * speed)
int api_moveSlide(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c)
        return 0;
    physicsMoveSlide(c->id,
                     (float)luaL_checknumber(L, 1),
                     (float)luaL_checknumber(L, 2));
    return 0;
}

/// is_on_floor() → 是否站在地面（跳跃判定一行搞定，不用手写 raycast）
int api_isOnFloor(lua_State *L)
{
    auto *c = getCtx(L);
    lua_pushboolean(L, (c && physicsIsOnFloor(c->id)) ? 1 : 0);
    return 1;
}

/// raycast(ox,oy,oz, dx,dy,dz [, maxDist=100]) — 玩法射线（走 Jolt 物理世界）
/// 命中返回 table: { entity, x, y, z, dist }；未命中返回 nil
/// 用途：子弹命中判定 / 敌人视线检测 / 脚下地面检测
int api_raycast(lua_State *L)
{
    float ox = (float)luaL_checknumber(L, 1);
    float oy = (float)luaL_checknumber(L, 2);
    float oz = (float)luaL_checknumber(L, 3);
    float dx = (float)luaL_checknumber(L, 4);
    float dy = (float)luaL_checknumber(L, 5);
    float dz = (float)luaL_checknumber(L, 6);
    float maxDist = (float)luaL_optnumber(L, 7, 100.0);

    flecs::entity_t hitId = 0;
    float hx, hy, hz, hd;
    // ★ 自动忽略发射者自己：脚本射线 99% 不想打中自己（落地检测/开枪），
    //   否则从自身中心发射 fraction=0 命中自己（曾致跳跃永远失败）
    auto *c = getCtx(L);
    flecs::entity_t selfId = c ? c->id : 0;
    if (!physicsRaycast(ox, oy, oz, dx, dy, dz, maxDist, hitId, hx, hy, hz, hd, selfId))
    {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)hitId);
    lua_setfield(L, -2, "entity");
    lua_pushnumber(L, hx);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, hy);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, hz);
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, hd);
    lua_setfield(L, -2, "dist");
    return 1;
}

// ---------------- 玩家存档（对标 Godot user:// + ConfigFile） ----------------
// 引擎只提供存取工具，存什么由游戏脚本决定（机制与策略分离）

/// save_set(key, value) — 写入存档草稿（内存）；支持 number/string/bool
int api_saveSet(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    nlohmann::json v;
    if (lua_isboolean(L, 2))
        v = (bool)lua_toboolean(L, 2);
    else if (lua_isnumber(L, 2))
        v = (double)lua_tonumber(L, 2);
    else if (lua_isstring(L, 2))
        v = std::string(lua_tostring(L, 2));
    else
        return luaL_error(L, "save_set: value 只支持 number/string/bool");
    SaveGame::instance().set(key, v);
    return 0;
}

/// save_get(key, default) → value — 从存档草稿读；无此键返回 default
int api_saveGet(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    nlohmann::json v = SaveGame::instance().get(key);
    if (v.is_boolean())
        lua_pushboolean(L, v.get<bool>());
    else if (v.is_number())
        lua_pushnumber(L, v.get<double>());
    else if (v.is_string())
        lua_pushstring(L, v.get<std::string>().c_str());
    else if (lua_gettop(L) >= 2)
        lua_pushvalue(L, 2); // 键不存在 → 返回调用方给的默认值
    else
        lua_pushnil(L);
    return 1;
}

/// save_write([slot=1]) → bool — 草稿落盘（玩家按"保存"时调）
int api_saveWrite(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().write(slot));
    return 1;
}

/// save_load([slot=1]) → bool — 磁盘存档读入草稿（读档/继续游戏）
int api_saveLoad(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().load(slot));
    return 1;
}

/// save_exists([slot=1]) → bool — 存档是否存在（主菜单"继续"按钮判断）
int api_saveExists(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().exists(slot));
    return 1;
}

// ---------------- 多场景（对标 Godot change_scene） ----------------

/// change_scene("scenes/level2.json") — 请求切换场景（帧末生效）
/// ★ 延迟执行：脚本正在被遍历执行，立刻删所有实体会崩（deferred 同 Godot）
int api_changeScene(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    SceneRouter::instance().requestChange(path);
    return 0;
}

/// get_scene() → 当前场景相对路径（判断自己在哪关）
int api_getScene(lua_State *L)
{
    lua_pushstring(L, SceneRouter::instance().current().c_str());
    return 1;
}

// ---------------- 音频（miniaudio 后端，对标 Godot AudioServer） ----------------
// 路径参数用相对资源根（如 "sounds/jump.wav"），运行时拼绝对路径 → 导出有效

/// play_sound("sounds/x.wav" [, volume=1]) — 播一次性音效（爆炸/UI 音）
int api_playSound(lua_State *L)
{
    const char *rel = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    std::string abs = (GamePaths::root() + QString::fromUtf8(rel)).toStdString();
    lua_pushboolean(L, audioPlaySound(abs, vol));
    return 1;
}

/// play_music("music/bgm.mp3" [, volume=1]) — 播/切背景音乐（循环，单例）
int api_playMusic(lua_State *L)
{
    const char *rel = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    std::string abs = (GamePaths::root() + QString::fromUtf8(rel)).toStdString();
    lua_pushboolean(L, audioPlayMusic(abs, vol));
    return 1;
}

/// stop_music() — 停止背景音乐
int api_stopMusic(lua_State *L)
{
    (void)L;
    audioStopMusic();
    return 0;
}

/// set_master_volume(v) — 主音量 0~1（影响全部声音）
int api_setMasterVolume(lua_State *L)
{
    audioSetMasterVolume((float)luaL_checknumber(L, 1));
    return 0;
}

/// set_bus_volume("music"/"sfx"/"master", v) — 总线音量（对标 Godot AudioBus）
/// 音乐和音效独立控制：设置界面的"音乐音量/音效音量"滑块直接对接
int api_setBusVolume(lua_State *L)
{
    const char *bus = luaL_checkstring(L, 1);
    audioSetBusVolume(bus, (float)luaL_checknumber(L, 2));
    return 0;
}

// ---------------- 游戏相机 / 环境 ----------------

/// set_camera(px, py, pz [, yaw, pitch]) — 每帧驱动游戏相机（第一/第三人称）
/// 找场景里的游戏相机（isEditor=false）更新其位置和朝向；省略角度则保持。
int api_setCamera(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    float px = (float)luaL_checknumber(L, 1);
    float py = (float)luaL_checknumber(L, 2);
    float pz = (float)luaL_checknumber(L, 3);
    bool hasYaw = !lua_isnoneornil(L, 4);
    bool hasPitch = !lua_isnoneornil(L, 5);
    float yaw = (float)luaL_optnumber(L, 4, 0);
    float pitch = (float)luaL_optnumber(L, 5, 0);
    c->w->each([&](CameraComponent &cam)
               {
        if (cam.isEditor) return; // 只驱动游戏相机，不碰编辑器飞行相机
        cam.position = {px, py, pz};
        if (hasYaw) cam.yaw = yaw;
        if (hasPitch) cam.pitch = pitch;
        // ★ 当帧重算 viewMatrix（消除 1 帧视角滞后）：
        //   CameraSystem(phase 50) 在脚本(100)之前已算过矩阵，
        //   不在这里重算的话，本帧渲染用的是"上一帧的相机位置"——
        //   移动/转头时画面永远慢一拍（果冻感的根源）
        float yr = glm::radians(cam.yaw);
        float pr = glm::radians(cam.pitch);
        glm::vec3 front = glm::normalize(
            glm::vec3(cos(yr) * cos(pr), sin(pr), sin(yr) * cos(pr)));
        cam.viewMatrix = glm::lookAt(cam.position, cam.position + front,
                                     glm::vec3(0, 1, 0)); });
    return 0;
}

// 内部：取场景 EnvironmentComponent（无则新建一个实体挂上），回调里改它。
// 让脚本能动态改环境（昼夜/雾区），RenderWidget 每帧读同一个组件。
template <typename F>
static void withEnvironment(LuaCtx *c, F &&fn)
{
    bool found = false;
    c->w->each([&](EnvironmentComponent &env)
               { fn(env); found = true; });
    if (!found)
    {
        EnvironmentComponent env{};
        fn(env);
        c->w->entity().set<EnvironmentComponent>(env); // 首次调用建一个环境实体
    }
}

/// set_fog(r, g, b, density) — 动态雾（走进山洞起雾等）。density 0=关闭
int api_setFog(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    float density = (float)luaL_checknumber(L, 4);
    withEnvironment(c, [&](EnvironmentComponent &env)
                    { env.fogColor = {r, g, b}; env.fogDensity = density; });
    return 0;
}

/// set_exposure(v) — 动态曝光（昼夜明暗）。1=正常，<1 变暗，>1 变亮
int api_setExposure(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    float v = (float)luaL_checknumber(L, 1);
    withEnvironment(c, [&](EnvironmentComponent &env)
                    { env.exposure = v; });
    return 0;
}

// ---------------- Tween 补间（对标 Godot Tween） ----------------

/// tween(id, "position"/"rotation"/"scale", x,y,z, duration
///       [, easing="linear" [, done_signal]])
/// 一行平滑动画：门滑开/平台移动/镜头拉近。完成时可给实体发信号。
/// easing: "linear"/"ease_in"/"ease_out"/"ease_in_out"/"bounce"/"elastic"
int api_tween(lua_State *L)
{
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    const char *target = luaL_checkstring(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    float z = (float)luaL_checknumber(L, 5);
    float duration = (float)luaL_optnumber(L, 6, 0.5);
    const char *easingName = luaL_optstring(L, 7, "linear");
    const char *doneSignal = luaL_optstring(L, 8, "");

    // 缓动名 → Easing.h 枚举（只暴露最常用的六种，够用且好记）
    int easing = 0; // kLinear
    if (strcmp(easingName, "ease_in") == 0)
        easing = 1; // kEaseInQuad
    else if (strcmp(easingName, "ease_out") == 0)
        easing = 2; // kEaseOutQuad
    else if (strcmp(easingName, "ease_in_out") == 0)
        easing = 3; // kEaseInOutQuad
    else if (strcmp(easingName, "bounce") == 0)
        easing = 10; // kEaseOutBounce
    else if (strcmp(easingName, "elastic") == 0)
        easing = 11; // kEaseOutElastic

    tweenStart(id, target, x, y, z, duration, easing, doneSignal);
    return 0;
}

// ---------------- 寻路（NavGrid，对标 Godot NavigationAgent） ----------------

/// nav_bake() — 烘焙导航网格：扫描全部【静态】碰撞体标记为障碍。
/// 进关卡时（bootstrap ready()）调一次；场景大改后可重调。
/// 触发器（isTrigger）不算障碍——传送门/感应区不该挡路。
int api_navBake(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    NavGrid::instance().clear();
    int count = 0;
    c->w->each([&](flecs::entity, const TransformComponent &tf,
                   const RigidbodyComponent &rb, const BoxColliderComponent &box)
               {
        if (rb.motionType != 0 || box.isTrigger) return; // 只算静态实体碰撞体
        glm::vec3 pos(tf.worldMatrix[3]);
        // 地面这类"又扁又大"的静态体不算障碍（顶面在脚下，不挡水平行走）
        float topY = pos.y + box.halfExtents.y * tf.scale.y;
        if (topY < 0.3f) return;
        float hx = box.halfExtents.x * tf.scale.x;
        float hz = box.halfExtents.z * tf.scale.z;
        NavGrid::instance().blockRect(pos.x - hx, pos.z - hz, pos.x + hx, pos.z + hz);
        ++count; });
    lua_pushinteger(L, count); // 返回烘焙的障碍数（调试用）
    return 1;
}

/// nav_find_path(fx, fz, tx, tz) → { {x=..,z=..}, ... } 或 nil（不可达）
/// 敌人追玩家：path = nav_find_path(ex, ez, px, pz)，朝 path[1] 移动
int api_navFindPath(lua_State *L)
{
    float fx = (float)luaL_checknumber(L, 1);
    float fz = (float)luaL_checknumber(L, 2);
    float tx = (float)luaL_checknumber(L, 3);
    float tz = (float)luaL_checknumber(L, 4);
    auto path = NavGrid::instance().findPath(fx, fz, tx, tz);
    if (path.empty())
    {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, (int)path.size(), 0);
    for (size_t i = 0; i < path.size(); ++i)
    {
        lua_createtable(L, 0, 2);
        lua_pushnumber(L, path[i].first);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, path[i].second);
        lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, (int)i + 1);
    }
    return 1;
}

/// nav_is_blocked(x, z) → 该位置是否是障碍（放置检查/AI 判断用）
int api_navIsBlocked(lua_State *L)
{
    float x = (float)luaL_checknumber(L, 1);
    float z = (float)luaL_checknumber(L, 2);
    lua_pushboolean(L, NavGrid::instance().isBlocked(x, z) ? 1 : 0);
    return 1;
}
