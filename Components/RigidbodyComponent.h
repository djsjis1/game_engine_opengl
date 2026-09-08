#ifndef RIGIDBODYCOMPONENT_H
#define RIGIDBODYCOMPONENT_H

#include <cstdint>
#include "Components/Reflect.h"

// ============================================================
// RigidbodyComponent — 刚体物理（由 Jolt Physics 驱动）
//
// motionType:
//   0 = Static   静态（地面、墙——不动，不受力）
//   1 = Kinematic 运动学（平台——脚本控制移动，推开别人自己不受力）
//   2 = Dynamic  动态（箱子、角色——受重力和碰撞）
//
// bodyId: Jolt 内部的 BodyID（PhysicsSystem 创建后写入，0=未创建）
// ============================================================
struct RigidbodyComponent
{
    REFLECT int motionType = 2;       // 默认动态刹体
    REFLECT float mass = 1.0f;        // 质量（仅 Dynamic 有效）
    REFLECT float friction = 0.5f;    // 摩擦系数 [0,1]
    REFLECT float restitution = 0.2f; // 弹性系数 [0=不弹, 1=完全弹]

    // 运行时数据（PhysicsSystem 管理，不序列化）
    uint32_t bodyId = 0; // Jolt BodyID（0 表示还没创建）
};

#endif
