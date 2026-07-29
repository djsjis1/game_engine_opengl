#ifndef SCRIPTSYSTEM_H
#define SCRIPTSYSTEM_H

#include "Core/System.h"
#include "Components/ScriptComponent.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include <map>

// ============================================================
// ScriptSystem — Lua 脚本引擎（1 个 lua_State + 每实体 table 环境）
//
// pImpl 模式：Impl 定义在 .cpp 中，头文件不依赖 lua.h。
// 使用裸指针（不用 unique_ptr）——这是 pImpl 的标准做法，
// 因为 unique_ptr 要求完整类型在析构点可见。
// ============================================================
class ScriptSystem : public System
{
public:
    ~ScriptSystem();
    void init(flecs::world &world);
    void shutdown() override;
    void update(flecs::world &world, float dt) override;

    int phase() const override { return 100; }
    int activeIn() const override { return (int)EngineState::Play; }

private:
    struct Impl;
    Impl *m_impl = nullptr;
};

#endif
