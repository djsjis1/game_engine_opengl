#ifndef TRANSFORMSYSTEM_H
#define TRANSFORMSYSTEM_H

#include "Core/System.h"
#include "Components/TransformComponent.h"

// ============================================================
// TransformSystem — 父子层级变换传播
//
// 使用 Flecs 原生 ChildOf 关系（e.child_of(parent)）。
//
// 每帧从根实体向下递归：
//   根实体:  worldMatrix = 局部矩阵
//   子实体:  worldMatrix = 父.worldMatrix × 局部矩阵
//
// phase=150：在所有逻辑（物理90/脚本100/旋转100）之后、渲染之前，
// 保证 worldMatrix 是本帧最终结果。
// ============================================================
class TransformSystem : public System
{
public:
    int phase() const override { return 150; }
    int activeIn() const override { return kAllStates; } // 编辑模式也要更新

    void update(flecs::world &world, float) override
    {
        // 只从"根"实体开始（没有带 Transform 的父实体），向下递归
        world.each([&](flecs::entity e, TransformComponent &)
                   {
            flecs::entity parent = e.parent();
            if (parent && parent.has<TransformComponent>())
                return; // 非根：由父实体的递归处理
            propagate(e, glm::mat4(1.0f), true); });
    }

private:
    /// 递归传播：算自己的世界矩阵 + 有效可见性 → 传给所有子实体
    void propagate(flecs::entity e, const glm::mat4 &parentWorld, bool parentVisible)
    {
        auto *tf = static_cast<TransformComponent *>(ecs_get_mut_id(
            e.world().c_ptr(), e.raw_id(),
            e.world().component<TransformComponent>().raw_id()));
        if (!tf)
            return;

        tf->updateMatrices(); // 局部矩阵
        tf->worldMatrix = parentWorld * tf->modelMatrix;
        tf->worldNormalMatrix = glm::mat3(glm::transpose(glm::inverse(tf->worldMatrix)));

        // 有效可见性：父可见 且 自己可见（父隐藏 → 整棵子树隐藏）
        tf->visibleInTree = parentVisible && tf->visible;

        // 递归所有子实体
        e.children([&](flecs::entity child)
                   { propagate(child, tf->worldMatrix, tf->visibleInTree); });
    }
};

#endif
