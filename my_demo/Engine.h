#ifndef ENGINE_H
#define ENGINE_H

// ============================================================
// Engine —— 引擎骨架（整个程序的"心脏"）
//
// 这个类只做三件事：
//   1. 持有 flecs::world —— 所有实体和组件的容器
//   2. 持有 System 列表 —— 按注册顺序执行
//   3. runFrame(dt) 每帧跑一遍所有 System
//
// 【使用方式】
//   Engine engine;
//   engine.addSystem(new MySystem);    // 注册系统
//   engine.runFrame(0.016f);           // 每帧调用
//   auto &w = engine.world();          // 获取 ECS 世界（编辑栏/渲染器用）
//
// 【扩展指南】
//   新增组件 → 在构造函数加一行 m_world.component<NewComponent>()
//   新增系统 → 在 MainWindow 里 engine.addSystem(new NewSystem)
// ============================================================

#include <vector>

// Qt 的 emit 宏和 Flecs 的 emit 变量冲突，包含 flecs.h 前必须禁用 emit
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include "System/System.h"
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/TagComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/CameraComponent.h"

class Engine
{
public:
    Engine()
    {
        // ---- 注册所有组件 ----
        // Flecs 需要知道每个组件的存在（用于查询、序列化等）
        // 调用 component<T>() 就是注册。后续 .set<T>() / .get<T>() 才有效
        m_world.component<TransformComponent>();
        m_world.component<MeshComponent>();
        m_world.component<MaterialComponent>();
        m_world.component<TagComponent>();
        m_world.component<RotateBehavior>();
        m_world.component<CameraComponent>();
        // 【扩展点】新增组件在这加一行
    }

    // ---- System 管线 ----
    // 系统按 addSystem 的顺序执行
    // 顺序很重要：比如 RotationSystem 必须在 render 之前
    void addSystem(System *sys) { m_systems.push_back(sys); }

    // ---- 主循环 ----
    // 每帧由 MainWindow 调用一次
    // dt = 帧间隔（秒），如 60FPS = 0.016s
    void runFrame(float dt)
    {
        for (auto *s : m_systems)
            s->update(m_world, dt);
    }

    // ---- 获取 ECS 世界 ----
    // 编辑栏和渲染器通过它读写实体和组件
    flecs::world &world() { return m_world; }

private:
    flecs::world m_world;            // Flecs ECS 世界（存所有数据）
    std::vector<System *> m_systems; // 系统列表（按注册顺序执行）
};

#endif
