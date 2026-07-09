#ifndef SCRIPTSYSTEM_H
#define SCRIPTSYSTEM_H

#include "System.h"
#include "../Components/ScriptComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/TagComponent.h"
#include <map>
#include <string>

// ============================================================
// ScriptSystem — Lua 脚本引擎（Godot 风格）
// setEnabled(false) 可在编辑模式暂停脚本
// ============================================================
class ScriptSystem : public System
{
public:
    void setEnabled(bool e) { m_enabled = e; }
    void update(flecs::world &world, float dt) override;

private:
    std::map<flecs::entity_t, void *> m_states;
    bool m_enabled = true;
};

#endif
#ifndef SCRIPTSYSTEM_H
#define SCRIPTSYSTEM_H

#include "System.h"
#include "../Components/ScriptComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/TagComponent.h"
#include <map>
#include <string>

// ============================================================
// ScriptSystem —— Lua 脚本引擎（Godot 风格）
//
// 功能：
//   1. 脚本编译缓存：相同 .lua 文件只编译一次，所有实体共享
//   2. 生命周期：ready() 首次激活时调用一次，update(dt) 每帧调用
//   3. 每个实体独立的 Lua 沙盒环境
//
// 所有 Lua 交互代码隔离在 System/LuaApi.cpp 中，
// 避免 Lua 宏和 Qt MOC 冲突。
// ============================================================
class ScriptSystem : public System
{
public:
    void update(flecs::world &world, float dt) override;

private:
    // 缓存的 Lua 状态（entity_id → lua_State*）
    std::map<flecs::entity_t, void *> m_states;
};

#endif
