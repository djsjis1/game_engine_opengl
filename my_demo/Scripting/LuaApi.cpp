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
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h" // physicsApplyImpulse / physicsSetVelocity
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Render/AnimationLibrary.h" // 脚本设计动画：anim_create/add_key/save
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/SignalComponent.h"
#include "Core/Input.h"
#include "Core/Time.h"
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
    if (!k[0])
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    char c = (k[0] >= 'a' && k[0] <= 'z') ? k[0] - 32 : k[0];
    lua_pushboolean(L, Input::instance().keyDown[(unsigned char)c]);
    return 1;
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
    if (!physicsRaycast(ox, oy, oz, dx, dy, dz, maxDist, hitId, hx, hy, hz, hd))
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
    // ---- 当前实体的 Transform 读写 ----
    lua_pushcfunction(L, api_getPosition);
    lua_setfield(L, -2, "get_position");
    lua_pushcfunction(L, api_setPosition);
    lua_setfield(L, -2, "set_position");
    lua_pushcfunction(L, api_getRotation);
    lua_setfield(L, -2, "get_rotation");
    lua_pushcfunction(L, api_setRotation);
    lua_setfield(L, -2, "set_rotation");
    lua_pushcfunction(L, api_getScale);
    lua_setfield(L, -2, "get_scale");
    lua_pushcfunction(L, api_setScale);
    lua_setfield(L, -2, "set_scale");
    lua_pushcfunction(L, api_setVisible);
    lua_setfield(L, -2, "set_visible");
    lua_pushcfunction(L, api_isVisible);
    lua_setfield(L, -2, "is_visible");

    // ---- 全局查询/创建 ----
    lua_pushcfunction(L, api_getNode);
    lua_setfield(L, -2, "get_node");
    lua_pushcfunction(L, api_isKeyPressed);
    lua_setfield(L, -2, "is_key_pressed");
    lua_pushcfunction(L, api_delta);
    lua_setfield(L, -2, "delta");
    lua_pushcfunction(L, api_createEntity);
    lua_setfield(L, -2, "create_entity");

    // ---- 跨实体操作 ----
    lua_pushcfunction(L, api_getNodePosition);
    lua_setfield(L, -2, "get_node_position");
    lua_pushcfunction(L, api_setNodePosition);
    lua_setfield(L, -2, "set_node_position");

    // ---- 信号系统 ----
    lua_pushcfunction(L, api_connect);
    lua_setfield(L, -2, "connect");
    lua_pushcfunction(L, api_emit);
    lua_setfield(L, -2, "emit");
    lua_pushcfunction(L, api_connectTo);
    lua_setfield(L, -2, "connect_to");
    lua_pushcfunction(L, api_disconnect);
    lua_setfield(L, -2, "disconnect");

    // ---- 多线程异步计算（Godot 风格 WorkerThreadPool）----
    lua_pushcfunction(L, api_asyncRun);
    lua_setfield(L, -2, "async");

    // ---- 脚本变量（Godot @export 风格）----
    lua_pushcfunction(L, api_export);
    lua_setfield(L, -2, "export");
    lua_pushcfunction(L, api_getProp);
    lua_setfield(L, -2, "get_prop");
    lua_pushcfunction(L, api_setProp);
    lua_setfield(L, -2, "set_prop");

    // ---- 物理（Jolt）----
    lua_pushcfunction(L, api_applyImpulse);
    lua_setfield(L, -2, "apply_impulse");
    lua_pushcfunction(L, api_setVelocity);
    lua_setfield(L, -2, "set_velocity");
    lua_pushcfunction(L, api_raycast);
    lua_setfield(L, -2, "raycast");

    // ---- 父子层级 ----
    lua_pushcfunction(L, api_setParent);
    lua_setfield(L, -2, "set_parent");
    lua_pushcfunction(L, api_getParent);
    lua_setfield(L, -2, "get_parent");

    // ---- 动画（Godot AnimationPlayer 风格）----
    lua_pushcfunction(L, api_animPlay);
    lua_setfield(L, -2, "anim_play");
    lua_pushcfunction(L, api_animStop);
    lua_setfield(L, -2, "anim_stop");
    lua_pushcfunction(L, api_animIsPlaying);
    lua_setfield(L, -2, "anim_is_playing");
    lua_pushcfunction(L, api_animSetSpeed);
    lua_setfield(L, -2, "anim_set_speed");
    lua_pushcfunction(L, api_animSetLoop);
    lua_setfield(L, -2, "anim_set_loop");

    // ---- 动画设计（脚本程序化创建，Godot Animation 资源 API 风格）----
    lua_pushcfunction(L, api_animSetFile);
    lua_setfield(L, -2, "anim_set_file");
    lua_pushcfunction(L, api_animCreate);
    lua_setfield(L, -2, "anim_create");
    lua_pushcfunction(L, api_animAddKey);
    lua_setfield(L, -2, "anim_add_key");
    lua_pushcfunction(L, api_animAddEvent);
    lua_setfield(L, -2, "anim_add_event");
    lua_pushcfunction(L, api_animSave);
    lua_setfield(L, -2, "anim_save");
}

// ============================================================
// ScriptSystem 实现在 ScriptEngine.cpp 中（按职责拆分）
// ============================================================
