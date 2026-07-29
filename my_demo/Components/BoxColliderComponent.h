#ifndef BOXCOLLIDERCOMPONENT_H
#define BOXCOLLIDERCOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"

// ============================================================
// BoxColliderComponent — 盒形碰撞体
//
// halfExtents: 盒子半尺寸（1,1,1 = 2x2x2 的盒子）
// 通常和 RigidbodyComponent 一起使用。
// ============================================================
struct BoxColliderComponent
{
    REFLECT glm::vec3 halfExtents{0.5f, 0.5f, 0.5f}; // 默认匹配 1x1x1 立方体
};

#endif
