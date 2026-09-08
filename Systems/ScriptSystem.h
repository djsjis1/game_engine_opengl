#ifndef SCRIPTSYSTEM_H
#define SCRIPTSYSTEM_H

#include "Core/System.h"
#include "Components/ScriptComponent.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include <map>
#include <string>

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

    /// 热重载：内置编辑器保存脚本后主动调用，立即生效（对标 Godot）
    /// 不依赖 mtime 轮询（整秒精度拓不准同秒保存）；清编译缓存 +
    /// 重置匹配该路径的实体 → 下帧 update 重新 init 并重跑 ready()
    void reloadScript(const std::string &path);

    /// 重置全部脚本环境（退出 Play 时调用）：释放所有实体的 Lua 环境，
    /// 下次进 Play 从零重建 → ready() 重跑、local 变量归零。
    /// 不重置的话第二次 Play 的 ready 不执行（set_mouse_mode/set_visible/
    /// yaw 初始化全在 ready 里）→ "第二次运行视角不正常"（实bug）
    void resetAllScripts();

    int phase() const override { return 100; }
    int activeIn() const override { return (int)EngineState::Play; }

private:
    struct Impl;
    Impl *m_impl = nullptr;
};

#endif
