// ============================================================
// LuaApi.cpp —— 1 全局 lua_State + per-entity table 沙盒
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
//     │                              ║  │   脚本里写:       ║
//     │  2. 调 ready() 或 update()─→ ║  │   function update(dt)
//     │                              ║  │     self:set_position(
//     │                              ║  │       0, dt*10, 0)
//     │                              ║  │   end             ║
//     │                              ║  └───────────────────║
//     │                              ║                      ║
//     │  3. API 读取 __active_ctx ←──║   __active_ctx ──→  ║
//     │     拿到 Transform* 指针     ║    指向 C++ 端内存   ║
//     │     直接修改 C++ 内存!       ║    (零拷贝!)          ║
//     │                              ╚══════════════════════╝
//
// ==================== 上下文传递 ====================
//
//   问题：Lua 函数被调用时，怎么知道"当前是哪个实体在执行"？
//
//   方案 A：每个 API 多传一个 entity_id 参数 → 脚本写起来丑
//   方案 B：用 Lua 注册表传上下文（本项目采用）→ 脚本简洁
//
//   实现：
//     1. ScriptSystem::update() 在调用 Lua 之前，把当前实体的
//        TransformComponent* 和 world* 放进注册表 __active_ctx
//     2. 每个 API 函数通过 getCtx(L) 取出这个指针
//     3. 直接读写 C++ 内存（零拷贝，无需序列化）
//
// ==================== 关键数据流 ====================
//
//   MainWindow::游戏循环(每 16ms)
//     │
//     │ Input::instance() ← 键鼠事件
//     │ Time::instance()  ← 帧计时
//     │
//     └→ Engine::runFrame(dt)
//          │
//          └→ ScriptSystem::update(world, dt)
//               │
//               │ 遍历 (Transform + ScriptComponent) 实体:
//               │
//               ├── 首次遇到 → initEntity() 创建沙盒 + 编译脚本
//               │
//               ├── 设置 __active_ctx = { &tf, &world, entityId }
//               │
//               ├── 首次 → callLifecycle("ready")  -- 脚本 ready()
//               │
//               └── 每帧 → callLifecycle("update", dt) -- 脚本 update(dt)
#include "LuaApi.h"
#include "LuaAsync.h"
#include "ScriptApiRegistry.h" // API 主表结构体 + scriptApiList() 声明
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h" // physicsApplyImpulse / physicsSetVelocity
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Render/AnimationLibrary.h" // 脚本设计动画：anim_create/add_key/save
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/SignalComponent.h"
#include "Components/MeshComponent.h"        // Lua 建造 API：add_mesh
#include "Components/MaterialComponent.h"    // add_material
#include "Components/RigidbodyComponent.h"   // add_rigidbody
#include "Components/BoxColliderComponent.h" // add_collider
#include "Components/CameraComponent.h"      // add_camera
#include "Components/RotateBehavior.h"       // add_rotate
#include "Render/ResourceManager.h"          // add_mesh 的资源句柄（纯记账，无 GL 时序要求）
#include "Core/GamePaths.h"                  // add_script 相对路径 → 运行时绝对路径
#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/SaveGame.h"    // 玩家存档（save_set/save_write 系列 API）
#include "Core/SceneRouter.h" // 运行时切场景（change_scene/get_scene）
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <mutex>
#include <queue>
#include <thread>

// ============================================================
// Lua 编译缓存（与 ScriptEngine.cpp 共享）
// ============================================================
std::map<std::string, CachedChunk> g_compiledChunks;

// ============================================================
// 多线程信号队列
//
// 工作线程不能直接调 Lua（lua_State 非线程安全），
// 所以 emit 从工作线程调用时，信号被推入此队列，
// 主线程在每帧开始时消费队列并调用回调。
// ============================================================
std::queue<DeferredSignal> g_signalQueue;
std::mutex g_signalMutex;
std::thread::id g_mainThreadId;

/// 工作线程调用：入队（线程安全）
static void enqueueSignal(flecs::entity_t emitter, const char *name, int nargs, lua_State *L)
{
    DeferredSignal ds;
    ds.emitterId = emitter;
    ds.signalName = name;
    // 从 Lua 栈取出额外参数（跳过第 1 个：信号名）
    for (int i = 2; i <= nargs + 1; ++i)
        ds.args.push_back((double)lua_tonumber(L, i));
    {
        std::lock_guard<std::mutex> lock(g_signalMutex);
        g_signalQueue.push(std::move(ds));
    }
}

/// 清空信号队列（丢弃未消费信号，进/出 Play 时防陈旧信号堆积）
void clearDeferredSignals()
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    std::queue<DeferredSignal> empty;
    std::swap(g_signalQueue, empty);
}

/// 主线程调用：消费全部排队信号
/// 必须在持有 lua_State 的线程上调用
void processDeferredSignals(lua_State *L, flecs::world &world)
{
    std::queue<DeferredSignal> local;
    {
        std::lock_guard<std::mutex> lock(g_signalMutex);
        std::swap(local, g_signalQueue);
    }
    while (!local.empty())
    {
        auto &ds = local.front();
        flecs::entity e = world.entity(ds.emitterId);
        if (!e)
        {
            local.pop();
            continue;
        } // 实体已删除，跳过

        auto compId = world.component<SignalComponent>().raw_id();
        auto *sig = (SignalComponent *)ecs_get_id(world.c_ptr(), ds.emitterId, compId);
        if (!sig)
        {
            local.pop();
            continue;
        }

        auto it = sig->signalMap.find(ds.signalName);
        if (it == sig->signalMap.end())
        {
            local.pop();
            continue;
        }

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
        local.pop();
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
    // ① 把键名 "__active_ctx" 压栈
    lua_pushstring(L, "__active_ctx");
    // ② 从注册表中取值：registry["__active_ctx"]
    lua_gettable(L, LUA_REGISTRYINDEX);
    // ③ 值是一个 lightuserdata（C++ 指针），转为 LuaCtx*
    auto *c = (LuaCtx *)lua_touserdata(L, -1);
    // ④ 弹出这个临时值，保持栈干净
    lua_pop(L, 1);
    return c;
}

// ============================================================
// 以下所有函数都是暴露给 Lua 的 C 函数
//
// 签名规范（Lua C API 要求）:
//   static int func(lua_State *L)
//   返回值 = 压入 Lua 栈的返回值个数
//
// 参数获取:
//   luaL_checknumber(L, 1) → 读第 1 个参数（Lua 传整数也自动转 float）
//   luaL_checkstring(L, 2) → 读第 2 个参数（字符串）
//
// 读取上下文:
//   auto *c = getCtx(L) → 拿到当前实体的 Transform*/world*/id
// ============================================================

/// get_position() → x, y, z
/// 从当前实体的 TransformComponent 中读取位置
static int api_getPosition(lua_State *L)
{
    auto *c = getCtx(L); // 拿当前实体的上下文
    if (!c || !c->tf)
    {
        return 0;
    } // 无上下文（如异步回调）或无 Transform
    lua_pushnumber(L, c->tf->position.x); // 结果 1: x
    lua_pushnumber(L, c->tf->position.y); // 结果 2: y
    lua_pushnumber(L, c->tf->position.z); // 结果 3: z
    return 3;                             // 告诉 Lua "返回了 3 个值"
}

/// set_position(x, y, z)
/// 直接修改 C++ 端 TransformComponent 内存（零拷贝！）
static int api_setPosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    c->tf->position.x = (float)luaL_checknumber(L, 1);
    c->tf->position.y = (float)luaL_checknumber(L, 2);
    c->tf->position.z = (float)luaL_checknumber(L, 3);
    // ★ 同步物理体：带 Dynamic 刚体的实体位置"真身"在 Jolt 里，
    //   只改 Transform 下一帧被物理回写拉回原地（读档/重生瞬移失效实bug）
    //   无刚体实体 physicsSetPosition 找不到 body 返回 false，无副作用
    physicsSetPosition(c->id, c->tf->position.x, c->tf->position.y, c->tf->position.z);
    return 0; // 无返回值
}

/// get_rotation() → x, y, z（欧拉角，度）
static int api_getRotation(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    lua_pushnumber(L, c->tf->rotation.x);
    lua_pushnumber(L, c->tf->rotation.y);
    lua_pushnumber(L, c->tf->rotation.z);
    return 3;
}

/// set_rotation(x, y, z)（欧拉角，度）
static int api_setRotation(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    c->tf->rotation.x = (float)luaL_checknumber(L, 1);
    c->tf->rotation.y = (float)luaL_checknumber(L, 2);
    c->tf->rotation.z = (float)luaL_checknumber(L, 3);
    return 0;
}
/// get_scale() → x, y, z
static int api_getScale(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    lua_pushnumber(L, c->tf->scale.x);
    lua_pushnumber(L, c->tf->scale.y);
    lua_pushnumber(L, c->tf->scale.z);
    return 3;
}
/// set_scale(x, y, z)
static int api_setScale(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    c->tf->scale.x = (float)luaL_checknumber(L, 1);
    c->tf->scale.y = (float)luaL_checknumber(L, 2);
    c->tf->scale.z = (float)luaL_checknumber(L, 3);
    return 0;
}

/// set_visible(true/false) — 显示/隐藏当前实体（对标 Godot node.visible）
static int api_setVisible(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        return 0;
    }
    c->tf->visible = lua_toboolean(L, 1) != 0;
    return 0;
}

/// is_visible() → true/false（返回用户设置的 visible）
static int api_isVisible(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, c->tf->visible ? 1 : 0);
    return 1;
}

/// get_node("实体名") → entity_id
/// 通过 TagComponent.name 查找实体，返回其 flecs ID
/// 用法：local cube = get_node("ScriptCube")
static int api_getNode(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    const char *name = luaL_checkstring(L, 1); // 要查找的名字
    flecs::entity_t foundId = 0;               // 0 表示未找到
    // 遍历所有有 TagComponent 的实体，匹配 name
    c->w->each([&](flecs::entity e, const TagComponent &tag)
               {
        if (strcmp(tag.name, name) == 0)
            foundId = e.raw_id(); }); // 记录找到的第一个
    if (foundId)
    {
        lua_pushinteger(L, (lua_Integer)foundId);
        return 1;
    }
    return 0; // 没找到 → Lua 端收到 nil
}

/// is_key_pressed("键名") → true/false
/// 直接从 Input 单例读取，无需手动注入
static int api_isKeyPressed(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    // 支持字母/数字 + 特殊键名（"space"/"left"/"shift"...）
    lua_pushboolean(L, Input::instance().isDown(Input::keyFromName(k)));
    return 1;
}

/// is_key_just_pressed("key") → 本帧刚按下（边沿，跳跃/开枪用）
static int api_isKeyJustPressed(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isJustPressed(Input::keyFromName(k)));
    return 1;
}

/// is_key_just_released("key") → 本帧刚松开（边沿）
static int api_isKeyJustReleased(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isJustReleased(Input::keyFromName(k)));
    return 1;
}

/// is_action_pressed("jump") → 动作是否按住（经动作映射，键位可配）
static int api_isActionPressed(lua_State *L)
{
    const char *a = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isActionDown(a));
    return 1;
}

/// is_action_just_pressed("jump") → 动作本帧刚触发（边沿）
static int api_isActionJustPressed(lua_State *L)
{
    const char *a = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isActionJustPressed(a));
    return 1;
}

// ---------------- 鼠标（对标 Godot Input） ----------------

/// is_mouse_pressed("left"/"middle"/"right") → 鼠标键是否按住（持续）
static int api_isMousePressed(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseDown(Input::mouseFromName(b)));
    return 1;
}

/// is_mouse_just_pressed("left") → 本帧刚按下（边沿，点击/射击用）
static int api_isMouseJustPressed(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseJustPressed(Input::mouseFromName(b)));
    return 1;
}

/// is_mouse_just_released("left") → 本帧刚松开（边沿，拖拽结束用）
static int api_isMouseJustReleased(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseJustReleased(Input::mouseFromName(b)));
    return 1;
}

/// get_mouse_position() → x, y（视口内像素坐标，配合 raycast 拾取）
static int api_getMousePosition(lua_State *L)
{
    lua_pushnumber(L, Input::instance().mouseX());
    lua_pushnumber(L, Input::instance().mouseY());
    return 2;
}

/// get_mouse_delta() → dx, dy（本帧相对位移，FPS 视角控制用）
static int api_getMouseDelta(lua_State *L)
{
    lua_pushnumber(L, Input::instance().mouseDeltaX());
    lua_pushnumber(L, Input::instance().mouseDeltaY());
    return 2;
}

/// get_mouse_wheel() → delta（本帧滚轮格数，上正下负；缩放/切武器用）
static int api_getMouseWheel(lua_State *L)
{
    lua_pushnumber(L, Input::instance().wheel());
    return 1;
}

/// set_mouse_mode("captured"/"visible") — 光标模式（Godot MOUSE_MODE 同款）
///   captured：隐藏光标+锁视口中心（FPS 视角）；visible：恢复正常
///   仅 Play 模式生效（编辑模式强制正常光标）；实际应用由 MainWindow 每帧执行
static int api_setMouseMode(lua_State *L)
{
    const char *m = luaL_checkstring(L, 1);
    Input::instance().setMouseMode(strcmp(m, "captured") == 0 ? 1 : 0);
    return 0;
}

/// delta() → dt（帧间隔，秒）
/// 直接从 Time 单例读取
static int api_delta(lua_State *L)
{
    lua_pushnumber(L, Time::instance().delta);
    return 1;
}

/// create_entity("名称") → entity_id
/// 在 ECS 世界中创建新实体（带 TagComponent + TransformComponent）
static int api_createEntity(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    const char *name = luaL_optstring(L, 1, "Entity"); // 第二个参数是默认值
    auto e = c->w->entity();                           // 创建空实体
    TagComponent tc{};
    strncpy(tc.name, name, sizeof(tc.name) - 1);
    e.set<TransformComponent>({}); // 必须有 Transform，否则不可见
    // TagComponent 用 C API 设置（Flecs MSVC 兼容性）
    e.set_ptr(c->w->component<TagComponent>().raw_id(), sizeof(TagComponent), &tc);
    lua_pushinteger(L, (lua_Integer)e.raw_id()); // 返回 entity ID
    return 1;
}

// ---------------- Lua 建造 API（纯代码造世界，LÖVE2D/Godot 纯脚本风格） ----------------
// create_entity 只生"光杆"实体（Tag+Transform），下面这组负责"装配"：
// 形状/颜色/物理/行为/相机——bootstrap 脚本在 ready() 里搭出整个游戏

/// 取"活着的"目标实体（建造 API 公共入口；死实体/0 → 返回 null 实体）
static flecs::entity buildTarget(lua_State *L, int argIdx)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return flecs::entity::null();
    flecs::entity_t id = (flecs::entity_t)luaL_checkinteger(L, argIdx);
    if (!id || !c->w->is_alive(id))
        return flecs::entity::null(); // 防御：脚本可能传过期 ID
    return c->w->entity(id);
}

/// add_mesh(id, "cube") — 给实体加可见网格（不加就是不可见的逻辑实体）
static int api_addMesh(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *type = luaL_optstring(L, 2, "cube");
    MeshComponent m{};
    // 句柄化：同名网格命中缓存共享 GPU 数据；acquire 纯记账（延迟创建）
    m.meshHandle = ResourceManager::instance().acquireMesh(type);
    strncpy(m.meshType, type, sizeof(m.meshType) - 1);
    e.set<MeshComponent>(m);
    return 0;
}

/// add_material(id, r, g, b) — 漫反射颜色（0~1）
static int api_addMaterial(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    MaterialComponent mat{};
    mat.diffuse = {(float)luaL_checknumber(L, 2),
                   (float)luaL_checknumber(L, 3),
                   (float)luaL_checknumber(L, 4)};
    e.set<MaterialComponent>(mat);
    return 0;
}

/// add_rigidbody(id, motionType) — 物理刚体（0=静态地面 1=运动学 2=动态）
static int api_addRigidbody(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    RigidbodyComponent rb{};
    rb.motionType = (int)luaL_optinteger(L, 2, 2);
    e.set<RigidbodyComponent>(rb);
    return 0;
}

/// add_collider(id [, hx, hy, hz]) — 盒碰撞体半尺寸（默认 0.5 配 1x1 立方体）
static int api_addCollider(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    BoxColliderComponent box{};
    box.halfExtents = {(float)luaL_optnumber(L, 2, 0.5),
                       (float)luaL_optnumber(L, 3, 0.5),
                       (float)luaL_optnumber(L, 4, 0.5)};
    e.set<BoxColliderComponent>(box);
    return 0;
}

/// add_script(id, "scripts/xxx.lua") — 挂行为脚本（相对资源根路径）
/// ★ 运行时拼绝对路径（开发=项目目录，导出=exe/assets/）→ 导出天然有效
static int api_addScript(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *rel = luaL_checkstring(L, 2);
    ScriptComponent sc{};
    QString abs = GamePaths::root() + QString::fromUtf8(rel);
    strncpy(sc.filePath, abs.toUtf8().constData(), sizeof(sc.filePath) - 1);
    e.set<ScriptComponent>(sc); // ScriptSystem 下帧自动初始化并跑 ready()
    return 0;
}

/// add_camera(id, px, py, pz [, yaw=-90, pitch=-30]) — 游戏相机（active）
static int api_addCamera(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    CameraComponent cam{};
    cam.position = {(float)luaL_checknumber(L, 2),
                    (float)luaL_checknumber(L, 3),
                    (float)luaL_checknumber(L, 4)};
    cam.yaw = (float)luaL_optnumber(L, 5, -90);
    cam.pitch = (float)luaL_optnumber(L, 6, -30);
    cam.isEditor = false; // 游戏相机：不吃编辑器 WASD
    cam.active = true;
    e.set<CameraComponent>(cam);
    return 0;
}

/// add_rotate(id, sx, sy, sz) — 自转装饰（度/秒，传送门那种效果）
static int api_addRotate(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    RotateBehavior rot{};
    rot.speedX = (float)luaL_optnumber(L, 2, 0);
    rot.speedY = (float)luaL_optnumber(L, 3, 0);
    rot.speedZ = (float)luaL_optnumber(L, 4, 0);
    e.set<RotateBehavior>(rot);
    return 0;
}

/// set_node_scale(id, x, y, z) — 缩放任意实体（配合 add_collider 记得同步）
static int api_setNodeScale(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    auto *tf = static_cast<TransformComponent *>(ecs_get_mut_id(
        e.world().c_ptr(), e.raw_id(),
        e.world().component<TransformComponent>().raw_id()));
    if (!tf)
        return 0;
    tf->scale = {(float)luaL_checknumber(L, 2),
                 (float)luaL_checknumber(L, 3),
                 (float)luaL_checknumber(L, 4)};
    return 0;
}

/// destroy_entity(id) — 删除实体（物理体/脚本环境/Mesh 引用由 OnRemove 自动清）
static int api_destroyEntity(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (e)
        e.destruct();
    return 0;
}

/// get_node_position(entity_id) → x, y, z
/// 读取任意实体的位置（不仅限于当前脚本实体）
/// entity_id 通常来自 get_node() 或 create_entity() 的返回值
static int api_getNodePosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1); // 目标实体 ID
    // 用 C API 查询（Flecs C++ e.get<T>() 在 MSVC 有 bug）
    auto *tf = (TransformComponent *)ecs_get_id(c->w->c_ptr(), eid,
                                                c->w->component<TransformComponent>().raw_id());
    if (!tf)
    {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L); // 没找到 → 返回 3 个 nil
        return 3;
    }
    lua_pushnumber(L, tf->position.x);
    lua_pushnumber(L, tf->position.y);
    lua_pushnumber(L, tf->position.z);
    return 3;
}

/// set_node_position(entity_id, x, y, z)
/// 设置任意实体的位置（跨实体操控）
static int api_setNodePosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1); // 目标实体 ID
    // ecs_get_mut_id 获取可写指针（加锁/标记脏）
    auto *tf = (TransformComponent *)ecs_get_mut_id(c->w->c_ptr(), eid,
                                                    c->w->component<TransformComponent>().raw_id());
    if (!tf)
        return 0; // 目标实体没 Transform，静默忽略
    tf->position.x = (float)luaL_checknumber(L, 2);
    tf->position.y = (float)luaL_checknumber(L, 3);
    tf->position.z = (float)luaL_checknumber(L, 4);
    // 同步物理体（同 set_position：防止被物理回写覆盖）
    physicsSetPosition(eid, tf->position.x, tf->position.y, tf->position.z);
    return 0;
}

// ============================================================
// 脚本变量 API（Godot @export 风格——编辑器可见）
//
// 变量存在 ScriptComponent.properties 中，与脚本路径同在一个组件。
// 编辑器选中 ScriptComponent 时同时显示脚本路径和 export 变量。
// ============================================================

static ScriptComponent *getScriptComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr; // 无上下文（如异步回调）
    auto compId = c->w->component<ScriptComponent>().raw_id();
    return (ScriptComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
}

/// export("name", default) — 定义变量，编辑器可见
static int api_export(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    auto *sc = getScriptComp(c);
    if (sc && sc->properties.find(name) == sc->properties.end())
        sc->properties[name] = val;
    return 0;
}

/// get_prop("name") → value
static int api_getProp(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    auto *sc = getScriptComp(c);
    if (sc)
    {
        auto it = sc->properties.find(name);
        if (it != sc->properties.end())
        {
            lua_pushnumber(L, it->second);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

/// set_prop("name", value)
static int api_setProp(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    auto *sc = getScriptComp(c);
    if (sc)
        sc->properties[name] = val;
    return 0;
}

// ============================================================
// 动画 API（对标 Godot AnimationPlayer.play/stop）
// ============================================================

/// 取当前实体的 AnimationComponent（可写），无则返回 null
static AnimationComponent *getAnimComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr;
    return (AnimationComponent *)ecs_get_mut_id(
        c->w->c_ptr(), c->id, c->w->component<AnimationComponent>().raw_id());
}

/// anim_play("clip名") — 从头播放指定动画（实体需已挂 AnimationComponent）
static int api_animPlay(lua_State *L)
{
    auto *c = getCtx(L);
    auto *ac = getAnimComp(c);
    if (!ac)
        return 0;
    const char *clip = luaL_checkstring(L, 1);
    strncpy(ac->current, clip, sizeof(ac->current) - 1);
    ac->current[sizeof(ac->current) - 1] = '\0';
    ac->time = 0.0f;
    ac->playing = true;
    return 0;
}

/// anim_crossfade("clip", duration) — 带过渡切换到新动画（对标 Godot crossfade）
///   旧动画不直接丢弃，而是在 duration 秒内从旧平滑混合到新（走→跑无缝过渡）
static int api_animCrossfade(lua_State *L)
{
    auto *c = getCtx(L);
    auto *ac = getAnimComp(c);
    if (!ac)
        return 0;
    const char *clip = luaL_checkstring(L, 1);
    float duration = (float)luaL_optnumber(L, 2, 0.3); // 默认 0.3s 过渡

    // 已在播该动画则忽略（避免重复触发过渡卡在开头）
    if (strncmp(ac->current, clip, sizeof(ac->current)) == 0)
        return 0;

    // 当前动画 → prev（淡出），新动画 → current（淡入，从头播）
    strncpy(ac->prev, ac->current, sizeof(ac->prev) - 1);
    ac->prev[sizeof(ac->prev) - 1] = '\0';
    ac->prevTime = ac->time; // 旧动画从当前位置继续淡出
    strncpy(ac->current, clip, sizeof(ac->current) - 1);
    ac->current[sizeof(ac->current) - 1] = '\0';
    ac->time = 0.0f;
    ac->blendTime = 0.0f;
    ac->blendDuration = duration > 0 ? duration : 0.0f;
    ac->playing = true;
    return 0;
}

/// anim_stop() — 停止播放（保持当前姿态，Godot 同行为）
static int api_animStop(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->playing = false;
    return 0;
}

/// anim_is_playing() → true/false
static int api_animIsPlaying(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    lua_pushboolean(L, (ac && ac->playing) ? 1 : 0);
    return 1;
}

/// anim_set_speed(倍率) — 1=正常 2=两倍速 负数=倒放
static int api_animSetSpeed(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->speed = (float)luaL_checknumber(L, 1);
    return 0;
}

/// anim_set_loop(true/false) — 循环开关
static int api_animSetLoop(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->loop = lua_toboolean(L, 1) != 0;
    return 0;
}

// ============================================================
// 动画【设计】API —— 脚本程序化创建动画
// （对标 Godot Animation 资源的 add_track/track_insert_key）
//
// 操作的是 AnimationLibrary 内存库 —— 与编辑器/播放系统同一份数据：
//   脚本创建的动画立即可 anim_play，也能在编辑器里看到/继续调；
//   anim_save() 后持久化为 .anim 文件
// ============================================================

/// 取（或自动添加）AnimationComponent —— 设计类 API 用，纯脚本流程不需要先在编辑器加组件
static AnimationComponent *getOrAddAnimComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr;
    auto compId = c->w->component<AnimationComponent>().raw_id();
    ecs_add_id(c->w->c_ptr(), c->id, compId); // 已有则无操作
    return (AnimationComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
}

/// anim_set_file("anims/xxx.anim") — 绑定动画文件（无组件时自动挂）
static int api_animSetFile(lua_State *L)
{
    auto *ac = getOrAddAnimComp(getCtx(L));
    if (!ac)
        return 0;
    const char *path = luaL_checkstring(L, 1);
    strncpy(ac->animFile, path, sizeof(ac->animFile) - 1);
    ac->animFile[sizeof(ac->animFile) - 1] = '\0';
    return 0;
}

/// anim_create("clip名", 时长秒) — 在当前动画文件里新建（或重置）一个动画
static int api_animCreate(lua_State *L)
{
    auto *ac = getOrAddAnimComp(getCtx(L));
    if (!ac || !ac->animFile[0])
        return 0; // 需先 anim_set_file（或编辑器里已填）
    const char *name = luaL_checkstring(L, 1);
    float duration = (float)luaL_optnumber(L, 2, 1.0);
    AnimLib *lib = AnimationLibraryManager::instance().getOrCreate(ac->animFile);
    if (!lib)
        return 0;
    AnimClip clip;
    clip.duration = duration;
    lib->clips[name] = clip; // 同名覆盖 = 重新设计
    return 0;
}

/// anim_add_key("clip", "轨道", 时间, x, y, z [, 缓动类型])
/// 轨道写法（Property Track）：
///   简写："position" / "rotation" / "scale"（自动映射到 TransformComponent）
///   全路径："MaterialComponent/diffuse"、"HealthComponent/hp" —— 任意 REFLECT 字段
///   标量字段（float/bool/int）只用 x 参数，y/z 传0即可
/// 缓动类型见 Easing.h 枚举（0=线性 10=弹跳 11=弹簧 12=自定义贝塞尔）
static int api_animAddKey(lua_State *L)
{
    auto *ac = getOrAddAnimComp(getCtx(L));
    if (!ac || !ac->animFile[0])
        return 0;
    const char *clipName = luaL_checkstring(L, 1);
    const char *target = luaL_checkstring(L, 2);
    float t = (float)luaL_checknumber(L, 3);
    glm::vec3 v((float)luaL_checknumber(L, 4),
                (float)luaL_checknumber(L, 5),
                (float)luaL_checknumber(L, 6));
    int easing = (int)luaL_optinteger(L, 7, 0);

    AnimLib *lib = AnimationLibraryManager::instance().getOrCreate(ac->animFile);
    if (!lib)
        return 0;
    auto it = lib->clips.find(clipName);
    if (it == lib->clips.end())
        return 0; // clip 不存在 → 先 anim_create

    // 解析轨道路径：含 '/' = 全路径；否则视为 Transform 字段简写
    std::string comp, field;
    const char *slash = strchr(target, '/');
    if (slash)
    {
        comp.assign(target, slash - target);
        field = slash + 1;
    }
    else
    {
        comp = "TransformComponent";
        field = target;
    }

    auto &track = it->second.track(comp, field);
    track.setKey(t, v, easing);
    track.invalidate(); // 新轨道首次采样前重新解析反射路径
    return 0;
}

/// anim_add_event("clip", 时间, "信号名") — 事件轨道（Method Track）
/// 播放头跨越该时刻时实体发射此信号，用 connect("信号名", cb) 接收
static int api_animAddEvent(lua_State *L)
{
    auto *ac = getOrAddAnimComp(getCtx(L));
    if (!ac || !ac->animFile[0])
        return 0;
    const char *clipName = luaL_checkstring(L, 1);
    float t = (float)luaL_checknumber(L, 2);
    const char *signal = luaL_checkstring(L, 3);
    AnimLib *lib = AnimationLibraryManager::instance().getOrCreate(ac->animFile);
    if (!lib)
        return 0;
    auto it = lib->clips.find(clipName);
    if (it != lib->clips.end())
        it->second.addEvent(t, signal);
    return 0;
}

/// anim_save() — 把内存动画库写回 .anim 文件（持久化，可选）
static int api_animSave(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (!ac || !ac->animFile[0])
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    bool ok = AnimationLibraryManager::instance().save(ac->animFile);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// ============================================================
// 父子层级 API（Flecs 原生 ChildOf 关系）
// ============================================================

/// set_parent(child_id, parent_id) — 设置父实体（parent_id=0 解除）
static int api_setParent(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto childId = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto parentId = (flecs::entity_t)luaL_optinteger(L, 2, 0);

    flecs::entity child = c->w->entity(childId);
    if (!child.is_alive())
        return 0;

    if (parentId == 0)
        child.remove(flecs::ChildOf, flecs::Wildcard); // 解除父子
    else
        child.child_of(c->w->entity(parentId)); // 成为 parent 的子实体
    return 0;
}

/// get_parent(entity_id) → parent_id 或 nil
static int api_getParent(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
    {
        lua_pushnil(L);
        return 1;
    }
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    flecs::entity e = c->w->entity(id);
    if (!e.is_alive())
    {
        lua_pushnil(L);
        return 1;
    }
    flecs::entity p = e.parent();
    if (p)
        lua_pushinteger(L, (lua_Integer)p.raw_id());
    else
        lua_pushnil(L);
    return 1;
}

// ============================================================
// 脚本物理 API（Jolt 驱动）
// ============================================================

/// apply_impulse(x, y, z) — 给当前实体施加冲量（如跳跃）
static int api_applyImpulse(lua_State *L)
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
static int api_setVelocity(lua_State *L)
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
/// 玩家移动标准写法：改水平速度、保留 y（重力/跳跃不被抹掉）
///   local vx,vy,vz = get_velocity(); set_velocity(mx, vy, mz)
static int api_getVelocity(lua_State *L)
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

// ---------------- 玩家存档（对标 Godot user:// + ConfigFile） ----------------
// 引擎只提供存取工具，存什么由游戏脚本决定（机制与策略分离）

/// save_set(key, value) — 写入存档草稿（内存）；支持 number/string/bool
static int api_saveSet(lua_State *L)
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
static int api_saveGet(lua_State *L)
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
static int api_saveWrite(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().write(slot));
    return 1;
}

/// save_load([slot=1]) → bool — 磁盘存档读入草稿（读档/继续游戏）
static int api_saveLoad(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().load(slot));
    return 1;
}

/// save_exists([slot=1]) → bool — 存档是否存在（主菜单"继续"按钮判断）
static int api_saveExists(lua_State *L)
{
    int slot = (int)luaL_optinteger(L, 1, 1);
    lua_pushboolean(L, SaveGame::instance().exists(slot));
    return 1;
}

// ---------------- 多场景（对标 Godot change_scene） ----------------

/// change_scene("scenes/level2.json") — 请求切换场景（帧末生效）
/// ★ 延迟执行：脚本正在被遍历执行，立刻删所有实体会崩（deferred 同 Godot）
static int api_changeScene(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    SceneRouter::instance().requestChange(path);
    return 0;
}

/// get_scene() → 当前场景相对路径（如 "scenes/level1.json"）
/// 同一个玩家脚本能判断自己在哪关（传送门去向、存档记录关卡用）
static int api_getScene(lua_State *L)
{
    lua_pushstring(L, SceneRouter::instance().current().c_str());
    return 1;
}

/// raycast(ox,oy,oz, dx,dy,dz [, maxDist=100]) — 玩法射线（走 Jolt 物理世界）
/// 命中返回 table: { entity, x, y, z, dist }；未命中返回 nil
/// 只打得中有碰撞体的实体（Rigidbody+BoxCollider），且仅 Play 模式有效
/// 用途：子弹命中判定 / 敌人视线检测 / 脚下地面检测
static int api_raycast(lua_State *L)
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

// ============================================================
// Godot 风格信号系统 —— 实体间事件通信
//
// 对标 Godot Engine 的 Object.connect / emit 机制：
//   connect(signalName, callback)    — 本实体监听自己的信号
//   emit(signalName, ...)            — 本实体发出信号，触发所有监听者
//   connect_to(entityId, name, cb)   — 跨实体：监听别人的信号
//
// 数据存储在 SignalComponent 中（每个实体一份）。
// ============================================================

/// 获取或创建当前实体的 SignalComponent
/// 如果实体还没有信号组件，自动添加一个空的
static SignalComponent *getOrAddSignal(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr; // 无上下文（如异步回调）
    auto compId = c->w->component<SignalComponent>().raw_id();
    // 尝试获取可写指针
    auto *sig = (SignalComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
    if (!sig)
    {
        // 没有 → 先添加组件，再获取指针
        c->w->entity(c->id).set<SignalComponent>({});
        sig = (SignalComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
    }
    return sig;
}

/// connect("信号名", callback)
///
/// 本实体监听自己的信号。当 emit("信号名", ...) 时，callback 被调用。
///
/// 内部流程:
///   1. 将 Lua 函数保存到注册表（luaL_ref → 返回整数引用 ref）
///   2. 创建 SignalConnection{listenerId, ref} 写入 signalMap
///   （emit 时通过 lua_rawgeti(REGISTRY, ref) 取出回调，不污染全局表）
static int api_connect(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1); // 信号名
    luaL_checktype(L, 2, LUA_TFUNCTION);       // 必须是函数
    auto *sig = getOrAddSignal(c);
    if (!sig)
        return 0;

    // ① 把回调函数保存到注册表（拿到 ref 整数索引）
    lua_pushvalue(L, 2);                      // 复制函数到栈顶
    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // 存入注册表，返回整数引用

    // ② 创建连接记录（存 ref，不存全局表字符串）
    SignalConnection conn{c->id, ref};
    sig->signalMap[name].push_back(conn);

    lua_pushinteger(L, ref);
    return 1;
}

/// emit("信号名", 参数1, 参数2, ...)
///
/// 遍历 signalMap["信号名"] 里的所有监听者，逐一调用它们的回调。
///
/// 例如：emit("damaged", 50) →
///   查 signalMap["damaged"] → [{B, "c3"}, {C, "c7"}]
///   → 调 _G["c3"](50) → 调 _G["c7"](50)
static int api_emit(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L) - 1;

    // 非主线程 → 入队延迟执行（不能直接调 Lua）
    if (std::this_thread::get_id() != g_mainThreadId)
    {
        enqueueSignal(c->id, name, nargs, L);
        return 0;
    }

    // 主线程 → 立刻同步执行
    auto *sig = getOrAddSignal(c);
    if (!sig)
        return 0;

    auto it = sig->signalMap.find(name);
    if (it == sig->signalMap.end())
        return 0;

    for (auto &conn : it->second)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, conn.callbackRef);
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }
        for (int i = 0; i < nargs; ++i)
            lua_pushvalue(L, i + 2);
        if (lua_pcall(L, nargs, 0, 0) != LUA_OK)
            lua_pop(L, 1);
    }
    return 0;
}

/// connect_to(目标实体ID, "信号名", callback)
///
/// 跨实体监听！让"我"去监听"另一个实体"的信号。
///
/// 与 connect 的区别:
///   connect("hit", cb)          → 监听 自己 的信号
///   connect_to(实体B, "hit", cb) → 监听 实体B 的信号
///
/// 关键: 要在我的 listensTo 中记录"我监听了 B 的 hit"，
///       这样我删除时能去 B 的 signalMap 中清理我的记录。
static int api_connectTo(lua_State *L)
{
    auto *c = getCtx(L);
    auto tid = (flecs::entity_t)luaL_checkinteger(L, 1); // 目标实体 ID
    const char *name = luaL_checkstring(L, 2);           // 信号名
    luaL_checktype(L, 3, LUA_TFUNCTION);                 // 回调函数

    // ① 读写目标实体 B 的 SignalComponent
    auto *sig = (SignalComponent *)ecs_get_mut_id(
        c->w->c_ptr(), tid, c->w->component<SignalComponent>().raw_id());
    if (!sig)
    {
        // B 还没有信号组件 → 帮它创建一个
        c->w->entity(tid).set<SignalComponent>({});
        sig = (SignalComponent *)ecs_get_mut_id(
            c->w->c_ptr(), tid, c->w->component<SignalComponent>().raw_id());
    }
    if (!sig)
        return 0;

    // ② 保存回调函数到注册表
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    SignalConnection conn{c->id, ref};
    sig->signalMap[name].push_back(conn); // 写入 B 的 signalMap

    // ③ ★ 反向索引：在我的 listensTo 中记录"我监听了 B"
    auto *mySig = getOrAddSignal(c);
    if (mySig)
        mySig->listensTo[tid].push_back(name);
    //          ↑
    //          实体 B 的 ID → ["信号名", ...]
    //          用于我删除时反向清理 B 的 signalMap

    lua_pushinteger(L, ref);
    return 1;
}

/// disconnect("信号名" [, ref])
///
/// 取消信号监听。
///   disconnect("hit")        → 取消本实体对 "hit" 的全部监听
///   disconnect("hit", ref)   → 取消指定回调（ref 是 connect 的返回值）
///
/// 内部：从 signalMap 中移除匹配的连接 → luaL_unref 释放回调 →
///       如果是跨实体连接，从 listensTo 中清理反向索引。
static int api_disconnect(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    int targetRef = lua_gettop(L) >= 2 ? (int)luaL_checkinteger(L, 2) : -1;

    auto compId = c->w->component<SignalComponent>().raw_id();
    auto *sig = (SignalComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
    if (!sig)
        return 0;

    auto it = sig->signalMap.find(name);
    if (it == sig->signalMap.end())
        return 0;

    auto &conns = it->second;
    for (auto ci = conns.begin(); ci != conns.end();)
    {
        bool match = (targetRef < 0) || (ci->callbackRef == targetRef);
        if (!match)
        {
            ++ci;
            continue;
        }

        // 释放 Lua 回调（注册表引用 -1）
        luaL_unref(L, LUA_REGISTRYINDEX, ci->callbackRef);

        // 清理反向索引：从 listensTo 中移除对应源实体的这条信号
        auto &lt = sig->listensTo;
        for (auto li = lt.begin(); li != lt.end();)
        {
            auto &names = li->second;
            names.erase(std::remove(names.begin(), names.end(), name), names.end());
            if (names.empty())
                li = lt.erase(li);
            else
                ++li;
        }

        ci = conns.erase(ci);
    }

    // 该信号名下没有连接了 → 清理空条目
    if (conns.empty())
        sig->signalMap.erase(it);

    return 0;
}

// ============================================================
// injectApi —— 把 C++ 函数注入到 Lua 沙盒 table 中
//
// 每个脚本实体的沙盒 table 创建时会调用此函数，
// 把 get_position、set_position 等 C 函数注入为 table 的方法。
//
// 【脚本可用的全部 API 清单】
//   — Transform 读写 —
//     get_position()/set_position(x,y,z)
//     get_rotation()/set_rotation(x,y,z)   — 欧拉角，度
//     get_scale()/set_scale(x,y,z)
//   — 全局查询/创建 —
//     get_node("名字") → entity_id
//     create_entity("名字") → entity_id
//     is_key_pressed("W") → bool
//     delta() → 帧间隔秒数
//   — 跨实体操作 —
//     get_node_position(id) / set_node_position(id,x,y,z)
//   — 脚本变量（Godot @export） —
//     export("name",默认值) / get_prop("name") / set_prop("name",值)
//   — 物理（Jolt） —
//     apply_impulse(x,y,z)  — 施加冲量（跳跃）
//     set_velocity(x,y,z)   — 直接设速度
//     raycast(ox,oy,oz,dx,dy,dz[,maxDist]) — 物理射线（Jolt）
//       命中返回 {entity,x,y,z,dist}，未命中 nil（子弹/视线/地面检测）
//   — 父子层级 —
//     set_parent(child,parent) / get_parent(id)
//   — 动画播放（Godot AnimationPlayer） —
//     anim_play("clip") / anim_stop() / anim_is_playing()
//     anim_set_speed(倍率) / anim_set_loop(bool)
//   — 动画设计（Godot Animation 资源 API） —
//     anim_set_file("anims/x.anim") / anim_create("clip",时长)
//     anim_add_key("clip","position|rotation|scale",t,x,y,z[,缓动])
//     anim_add_key("clip","组件名/字段名",t,x,y,z[,缓动]) — 任意 REFLECT 字段
//     anim_add_event("clip",t,"信号名") — 事件轨道（播到该时刻发信号）
//     anim_save() — 持久化到文件
//   — 信号（Godot 风格） —
//     connect("信号",cb) / emit("信号",...)
//     connect_to(id,"信号",cb) / disconnect("信号"[,ref])
//   — 多线程 —
//     async("lua代码", cb)  — 后台线程计算，主线程回调
//
// Lua 端调用方式（实体在自己的沙盒里，直接调）:
//   set_position(0, 10, 0)   — 不需要 self:，因为注入到了沙盒环境
//
// 内部机制:
//   lua_pushcfunction(L, api_getPosition)  — 把 C 函数压栈
//   lua_setfield(L, -2, "get_position")    — 设为 table 的字段
void injectApi(lua_State *L)
{
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
// 加一个新 API 只需：①写 api_xxx 实现 ②在本表加一行。
// injectApi 注册、engine_api.lua 存根、补全词库、高亮词表四处自动同步。
// func 填 nullptr 表示“只有元数据不注册”（如 Lua 内置 print）。
// ============================================================
const std::vector<ScriptApiEntry> &scriptApiList()
{
    static const std::vector<ScriptApiEntry> table = {
        // ---- 基础 ----
        {nullptr, "print", "...", "", "打印到控制台（Lua 内置）"},
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
        {api_addMaterial, "add_material", "id, r, g, b", "", "设置漫反射颜色（0~1）"},
        {api_addRigidbody, "add_rigidbody", "id, motion", "", "加刚体（0=静态 1=运动学 2=动态）"},
        {api_addCollider, "add_collider", "id, hx, hy, hz", "", "加盒碰撞体（半尺寸，默认0.5）"},
        {api_addScript, "add_script", "id, path", "", "挂行为脚本（相对路径如 \"scripts/x.lua\"）"},
        {api_addCamera, "add_camera", "id, px, py, pz, yaw, pitch", "", "加游戏相机（active）"},
        {api_addRotate, "add_rotate", "id, sx, sy, sz", "", "自转装饰（度/秒）"},
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
        {api_setNodePosition, "set_node_position", "id, x, y, z", "", "设置指定实体位置"},
        // ---- 物理（Jolt）----
        {api_applyImpulse, "apply_impulse", "x, y, z", "", "施加冲量（瞬间推力，如跳跃）"},
        {api_setVelocity, "set_velocity", "x, y, z", "", "直接设置刚体线速度"},
        {api_getVelocity, "get_velocity", "", "number vx, number vy, number vz",
         "读取刚体速度（移动时保留 y：set_velocity(mx, vy, mz)）"},
        // ---- 玩家存档（exe旁 saves/，存什么由脚本定）----
        {api_saveSet, "save_set", "key, value", "", "写存档草稿（number/string/bool）"},
        {api_saveGet, "save_get", "key, default", "any", "读存档草稿（无此键返回 default）"},
        {api_saveWrite, "save_write", "slot", "boolean", "草稿落盘到槽位（默认1），玩家按保存时调"},
        {api_saveLoad, "save_load", "slot", "boolean", "读档到草稿（默认槽1），继续游戏时调"},
        {api_saveExists, "save_exists", "slot", "boolean", "存档是否存在（主菜单'继续'判断）"},
        // ---- 多场景 ----
        {api_changeScene, "change_scene", "path", "", "切换场景（帧末生效），如 change_scene(\"scenes/level2.json\")"},
        {api_getScene, "get_scene", "", "string", "当前场景相对路径（判断自己在哪关）"},
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
    };
    return table;
}

// ============================================================
// ScriptSystem 实现在 ScriptEngine.cpp 中（按职责拆分）
// ============================================================
