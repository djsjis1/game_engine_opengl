// ============================================================
// LuaApiSignalAnim.cpp — 信号系统 + 动画类脚本 API
//
// 信号（Godot Object.connect/emit 同款）：
//   connect / emit / connect_to / disconnect
//   数据在 SignalComponent；实体销毁时的清理见 LuaApi.cpp
//   的 cleanupEntitySignals（OnRemove observer 驱动）
// 动画：播放控制（AnimationPlayer 风格）+ 程序化设计（add_track 风格）
// 拆分地图见 LuaApiInternal.h。
// ============================================================
#include "LuaApiInternal.h"
#include "Components/SignalComponent.h"
#include "Components/AnimationComponent.h"
#include "Render/AnimationLibrary.h"
#include "Systems/AnimFsmSystem.h" // 动画状态机（anim_fsm_* API）
#include "Core/GamePaths.h"       // anim_set_file 相对路径 → 运行时绝对路径
#include <cstring>
#include <thread>

// ---------------- 信号系统 ----------------

/// 获取或创建当前实体的 SignalComponent（没有则自动添加空的）
static SignalComponent *getOrAddSignal(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr; // 无上下文（如异步回调）
    auto compId = c->w->component<SignalComponent>().raw_id();
    auto *sig = (SignalComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
    if (!sig)
    {
        c->w->entity(c->id).set<SignalComponent>({});
        sig = (SignalComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
    }
    return sig;
}

/// connect("信号名", callback) — 本实体监听自己的信号
/// 流程：回调 luaL_ref 存注册表 → SignalConnection{listenerId, ref} 入 signalMap
int api_connect(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto *sig = getOrAddSignal(c);
    if (!sig)
        return 0;

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    SignalConnection conn{c->id, ref};
    sig->signalMap[name].push_back(conn);

    lua_pushinteger(L, ref);
    return 1;
}

/// emit("信号名", 参数...) — 遍历监听者逐一调用回调
/// 非主线程调用（如物理碰撞回调）自动入队，主线程每帧消费
int api_emit(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0; // 异步回调里无实体上下文（曾是空指针崩溃点）
    const char *name = luaL_checkstring(L, 1);
    int nargs = lua_gettop(L) - 1;

    // 非主线程 → 入队延迟执行（lua_State 非线程安全）
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

/// connect_to(目标实体ID, "信号名", callback) — 跨实体监听
/// 双向登记：目标的 signalMap 记连接 + 我的 listensTo 记反向索引
/// （我死时靠 listensTo 去目标那清理，防孤儿回调）
int api_connectTo(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0; // 无实体上下文——安全忽略
    auto tid = (flecs::entity_t)luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // 目标必须活着——对已销毁 ID set 组件会把死实体"复活"成僵尸
    if (!c->w->entity(tid).is_alive())
        return 0;

    // ① 读写目标实体 B 的 SignalComponent（没有则帮它创建）
    auto *sig = (SignalComponent *)ecs_get_mut_id(
        c->w->c_ptr(), tid, c->w->component<SignalComponent>().raw_id());
    if (!sig)
    {
        c->w->entity(tid).set<SignalComponent>({});
        sig = (SignalComponent *)ecs_get_mut_id(
            c->w->c_ptr(), tid, c->w->component<SignalComponent>().raw_id());
    }
    if (!sig)
        return 0;

    // ② 保存回调到注册表，连接写入 B 的 signalMap
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    SignalConnection conn{c->id, ref};
    sig->signalMap[name].push_back(conn);

    // ③ 反向索引：我的 listensTo 记录"我监听了 B"（我删除时反向清理用）
    auto *mySig = getOrAddSignal(c);
    if (mySig)
        mySig->listensTo[tid].push_back(name);

    lua_pushinteger(L, ref);
    return 1;
}

/// disconnect("信号名" [, ref]) — 断开本实体信号上的连接
/// 省略 ref 断开该信号全部连接；带 ref 只断指定那条
int api_disconnect(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0; // 无实体上下文（异步回调里调用）——安全忽略
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

        // ★ 反向索引清理：这条连接的监听者是 ci->listenerId（可能是别的
        //   实体 connect_to 过来的）——要去【监听者】的 listensTo[我] 里
        //   删掉这个信号名。注意方向：自己的 listensTo 是"我监听了谁"，
        //   清那个就反了（曾是实 bug：监听者记录残留 → 死亡时引用错乱）
        if (ci->listenerId != c->id) // 自己 connect 自己的不用跨实体清
        {
            auto *listenerSig = (SignalComponent *)ecs_get_mut_id(
                c->w->c_ptr(), ci->listenerId, compId);
            if (listenerSig)
            {
                auto li = listenerSig->listensTo.find(c->id);
                if (li != listenerSig->listensTo.end())
                {
                    auto &names = li->second;
                    names.erase(std::remove(names.begin(), names.end(), name),
                                names.end());
                    if (names.empty())
                        listenerSig->listensTo.erase(li);
                }
            }
        }

        ci = conns.erase(ci);
    }

    if (conns.empty())
        sig->signalMap.erase(it); // 该信号名下没有连接了 → 清理空条目

    return 0;
}

// ---------------- 动画播放（对标 Godot AnimationPlayer） ----------------

/// 取当前实体的 AnimationComponent（可写），无则返回 null
static AnimationComponent *getAnimComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr;
    return (AnimationComponent *)ecs_get_mut_id(
        c->w->c_ptr(), c->id, c->w->component<AnimationComponent>().raw_id());
}

/// anim_play("clip名") — 从头播放指定动画
int api_animPlay(lua_State *L)
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

/// anim_crossfade("clip", duration) — 带过渡切换（走→跑无缝混合）
int api_animCrossfade(lua_State *L)
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
    ac->prevTime = ac->time;
    strncpy(ac->current, clip, sizeof(ac->current) - 1);
    ac->current[sizeof(ac->current) - 1] = '\0';
    ac->time = 0.0f;
    ac->blendTime = 0.0f;
    ac->blendDuration = duration > 0 ? duration : 0.0f;
    ac->playing = true;
    return 0;
}

/// anim_stop() — 停止播放（保持当前姿态，Godot 同行为）
int api_animStop(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->playing = false;
    return 0;
}

/// anim_is_playing() → true/false
int api_animIsPlaying(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    lua_pushboolean(L, (ac && ac->playing) ? 1 : 0);
    return 1;
}

/// anim_set_speed(倍率) — 1=正常 2=两倍速 负数=倒放
int api_animSetSpeed(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->speed = (float)luaL_checknumber(L, 1);
    return 0;
}

/// anim_set_loop(true/false) — 循环开关
int api_animSetLoop(lua_State *L)
{
    auto *ac = getAnimComp(getCtx(L));
    if (ac)
        ac->loop = lua_toboolean(L, 1) != 0;
    return 0;
}

// ---------------- 动画设计（程序化创建，对标 Godot add_track） ----------------
// 操作 AnimationLibrary 内存库——与编辑器/播放系统同一份数据：
// 脚本创建的动画立即可 anim_play，anim_save() 后持久化 .anim

/// 取（或自动添加）AnimationComponent——纯脚本流程不需要先在编辑器加组件
static AnimationComponent *getOrAddAnimComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr;
    auto compId = c->w->component<AnimationComponent>().raw_id();
    ecs_add_id(c->w->c_ptr(), c->id, compId); // 已有则无操作
    return (AnimationComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
}

/// anim_set_file("anims/xxx.anim") — 绑定动画文件（无组件时自动挂）
/// ★ 相对路径拼 GamePaths::root()，绝对路径直接用（与 add_script 一致）
int api_animSetFile(lua_State *L)
{
    auto *ac = getOrAddAnimComp(getCtx(L));
    if (!ac)
        return 0;
    const char *path = luaL_checkstring(L, 1);
    // 绝对路径直接用，相对路径拼 GamePaths::root()
    QString p = QString::fromUtf8(path);
    QString abs = (p.size() >= 2 && p[1] == ':') || p.startsWith('/')
                      ? p
                      : GamePaths::root() + p;
    QByteArray utf8 = abs.toUtf8();
    strncpy(ac->animFile, utf8.constData(), sizeof(ac->animFile) - 1);
    ac->animFile[sizeof(ac->animFile) - 1] = '\0';
    return 0;
}

/// anim_create("clip名", 时长秒) — 新建（或重置）一个动画
int api_animCreate(lua_State *L)
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
/// 轨道写法：简写 "position"/"rotation"/"scale"，
/// 或全路径 "MaterialComponent/albedo" —— 任意 REFLECT 字段
int api_animAddKey(lua_State *L)
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

/// anim_add_event("clip", 时间, "信号名") — 事件轨道（播到 t 时发信号）
int api_animAddEvent(lua_State *L)
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

/// anim_save() — 把内存动画库写回 .anim 文件（持久化）
int api_animSave(lua_State *L)
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

// ---------------- 动画状态机（对标 Godot AnimationTree StateMachine） ----------------
// 声明规则一次 + 每帧喂参数，切换（带 crossfade）自动发生。用法见 AnimFsmSystem.h

/// anim_fsm_add(id, from, to, param, ">"/"<", threshold [, blend=0.2])
int api_animFsmAdd(lua_State *L)
{
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    const char *from = luaL_checkstring(L, 2);
    const char *to = luaL_checkstring(L, 3);
    const char *param = luaL_checkstring(L, 4);
    const char *op = luaL_checkstring(L, 5);
    float threshold = (float)luaL_checknumber(L, 6);
    float blend = (float)luaL_optnumber(L, 7, 0.2);
    animFsmAddTransition(id, from, to, param, op, threshold, blend);
    return 0;
}

/// anim_fsm_start(id, "idle") — 设初始状态并开始判定
int api_animFsmStart(lua_State *L)
{
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    animFsmStart(id, luaL_checkstring(L, 2));
    return 0;
}

/// anim_fsm_param(id, "speed", v) — 每帧喂参数（切换自动发生）
int api_animFsmParam(lua_State *L)
{
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    animFsmSetParam(id, luaL_checkstring(L, 2), (float)luaL_checknumber(L, 3));
    return 0;
}

/// anim_fsm_state(id) → 当前状态名（无状态机返回 nil）
int api_animFsmState(lua_State *L)
{
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    std::string s = animFsmState(id);
    if (s.empty())
        lua_pushnil(L);
    else
        lua_pushstring(L, s.c_str());
    return 1;
}
