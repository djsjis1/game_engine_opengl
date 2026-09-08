#ifndef BOXCOLLIDERCOMPONENT_H
#define BOXCOLLIDERCOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"

// ============================================================
// BoxColliderComponent — 盒形碰撞体
//
// halfExtents: 盒子半尺寸（1,1,1 = 2x2x2 的盒子）
// isTrigger:   触发器模式（对标 Godot Area3D）——不产生物理阻挡，
//              物体穿过时发信号：进入 "trigger_enter"、分离 "collision_end"
//              （宝箱感应/传送门/伤害区/终点判定全靠它）
// 通常和 RigidbodyComponent 一起使用。
// ============================================================
struct BoxColliderComponent
{
    REFLECT glm::vec3 halfExtents{0.5f, 0.5f, 0.5f}; // 默认匹配 1x1x1 立方体
    REFLECT bool isTrigger = false;                  // true=触发器（穿透+发信号）
};

#endif
