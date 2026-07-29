#ifndef ROTATIONSYSTEM_H
#define ROTATIONSYSTEM_H

#include "Core/System.h"
#include "Components/TransformComponent.h"
#include "Components/RotateBehavior.h"

// ============================================================
// RotationSystem —— 自动旋转
//
// 每帧遍历所有同时有 TransformComponent 和 RotateBehavior 的实体，
// 把 RotateBehavior 里设置的速度（度/秒）乘以 dt，累加到旋转角度上。
//
// 例如：RotateBehavior.speedY = 90 → 每秒绕 Y 轴转 90°
//
// 使用方式：编辑栏给实体添加 RotateBehavior 组件，填入速度即可，
//           不需要写任何代码。
// ============================================================
class RotationSystem : public System
{
public:
    int phase() const override { return 100; }           // 与 Script 同级
    int activeIn() const override { return kAllStates; } // 编辑和运行都转

    void update(flecs::world &world, float dt) override
    {
        // world.each 自动过滤：只遍历同时有这两个组件的实体
        // &tf  是引用（可修改），直接改 rotation 字段
        // &rb  是 const 引用（只读），只读取 speedX/Y/Z
        // dt   从 lambda 捕获，每帧间隔（约 0.016 秒）
        world.each([dt](TransformComponent &tf, const RotateBehavior &rb)
                   {
            tf.rotation.x += rb.speedX * dt;
            tf.rotation.y += rb.speedY * dt;
            tf.rotation.z += rb.speedZ * dt; });
    }
};

#endif
