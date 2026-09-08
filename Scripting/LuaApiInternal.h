#ifndef LUAAPI_INTERNAL_H
#define LUAAPI_INTERNAL_H

// ============================================================
// LuaApiInternal.h — Lua API 实现文件之间的内部头（不对外）
//
// 【为什么存在】LuaApi.cpp 曾是 1700+ 行 73 个 API 的巨文件，
// 按域拆成 4 个实现文件后，主表 scriptApiList()（在 LuaApi.cpp）
// 需要引用所有 api_ 函数 → 在这里集中声明。
//
// 【拆分地图】改哪类 API 去哪个文件：
//   LuaApiEntity.cpp    — 实体/Transform/建造/跨实体/父子/export 变量
//   LuaApiInput.cpp     — 键盘/鼠标/动作/光标模式/delta
//   LuaApiGameplay.cpp  — 物理/射线/存档/场景/音频/相机/环境
//   LuaApiSignalAnim.cpp— 信号系统/动画播放/动画设计
//   LuaApi.cpp          — 框架层：ctx/信号队列/清理/injectApi/主表
//   LuaAsync.cpp        — async（线程池，独立模块）
//
// 【外部接口】游戏代码只 include LuaApi.h，本头仅 Scripting/ 内部用
// ============================================================

#include "LuaApi.h" // lua_State / LuaCtx / getCtx

// ---- 跨实现文件共享（实现在 LuaApi.cpp）----

/// 工作线程 emit 入队（lua_State 非线程安全，主线程每帧消费）
void enqueueSignal(flecs::entity_t emitter, const char *name, int nargs, lua_State *L);

/// 主线程 id（emit 判断"当前在哪个线程"用）
extern std::thread::id g_mainThreadId;

// ---- LuaApiEntity.cpp ----
int api_getPosition(lua_State *L);
int api_setPosition(lua_State *L);
int api_getRotation(lua_State *L);
int api_setRotation(lua_State *L);
int api_getScale(lua_State *L);
int api_setScale(lua_State *L);
int api_setVisible(lua_State *L);
int api_isVisible(lua_State *L);
int api_getNode(lua_State *L);
int api_createEntity(lua_State *L);
int api_destroyEntity(lua_State *L);
int api_addMesh(lua_State *L);
int api_addMaterial(lua_State *L);
int api_addTexture(lua_State *L);
int api_addRigidbody(lua_State *L);
int api_addCollider(lua_State *L);
int api_addScript(lua_State *L);
int api_addCamera(lua_State *L);
int api_addRotate(lua_State *L);
int api_addLight(lua_State *L);
int api_addParticles(lua_State *L);
int api_particlesEmit(lua_State *L);
int api_setNodeScale(lua_State *L);
int api_getNodePosition(lua_State *L);
int api_getNodeRotation(lua_State *L);
int api_setNodePosition(lua_State *L);
int api_setParent(lua_State *L);
int api_getParent(lua_State *L);
int api_export(lua_State *L);
int api_getProp(lua_State *L);
int api_setProp(lua_State *L);

// ---- LuaApiInput.cpp ----
int api_delta(lua_State *L);
int api_isKeyPressed(lua_State *L);
int api_isKeyJustPressed(lua_State *L);
int api_isKeyJustReleased(lua_State *L);
int api_isActionPressed(lua_State *L);
int api_isActionJustPressed(lua_State *L);
int api_isMousePressed(lua_State *L);
int api_isMouseJustPressed(lua_State *L);
int api_isMouseJustReleased(lua_State *L);
int api_getMousePosition(lua_State *L);
int api_getMouseDelta(lua_State *L);
int api_getMouseWheel(lua_State *L);
int api_setMouseMode(lua_State *L);

// ---- LuaApiGameplay.cpp ----
int api_applyImpulse(lua_State *L);
int api_setVelocity(lua_State *L);
int api_getVelocity(lua_State *L);
int api_moveSlide(lua_State *L);
int api_isOnFloor(lua_State *L);
int api_raycast(lua_State *L);
int api_saveSet(lua_State *L);
int api_saveGet(lua_State *L);
int api_saveWrite(lua_State *L);
int api_saveLoad(lua_State *L);
int api_saveExists(lua_State *L);
int api_changeScene(lua_State *L);
int api_getScene(lua_State *L);
int api_playSound(lua_State *L);
int api_playMusic(lua_State *L);
int api_stopMusic(lua_State *L);
int api_setMasterVolume(lua_State *L);
int api_setBusVolume(lua_State *L);
int api_setCamera(lua_State *L);
int api_setFog(lua_State *L);
int api_setExposure(lua_State *L);
int api_tween(lua_State *L);
int api_navBake(lua_State *L);
int api_navFindPath(lua_State *L);
int api_navIsBlocked(lua_State *L);
int api_delay(lua_State *L);           // 实现在 LuaApi.cpp（定时器与框架层同文件）
int api_engineStats(lua_State *L);     // 运行时状态快照（Agent 排障）
int api_engineSelfCheck(lua_State *L); // 自检项运行（Agent 排障）

// ---- LuaApiSignalAnim.cpp ----
int api_connect(lua_State *L);
int api_emit(lua_State *L);
int api_connectTo(lua_State *L);
int api_disconnect(lua_State *L);
int api_animPlay(lua_State *L);
int api_animCrossfade(lua_State *L);
int api_animStop(lua_State *L);
int api_animIsPlaying(lua_State *L);
int api_animSetSpeed(lua_State *L);
int api_animSetLoop(lua_State *L);
int api_animSetFile(lua_State *L);
int api_animCreate(lua_State *L);
int api_animAddKey(lua_State *L);
int api_animAddEvent(lua_State *L);
int api_animSave(lua_State *L);
int api_animFsmAdd(lua_State *L);
int api_animFsmStart(lua_State *L);
int api_animFsmParam(lua_State *L);
int api_animFsmState(lua_State *L);

#endif
