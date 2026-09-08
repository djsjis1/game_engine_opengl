#ifndef LIGHTCOMPONENT_H
#define LIGHTCOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"

// ============================================================
// LightComponent — 光源（对标 Godot DirectionalLight3D / OmniLight3D）
//
// 【数据驱动渲染的关键】光照参数不再硬编码在 RenderWidget，而是
//   做成"光源实体"的组件：场景里放一个带 LightComponent 的实体，
//   编辑器 Inspector 调颜色/强度/方向 → RenderWidget 每帧查询 → 喂 shader。
//   这就是 Godot 的 Light 节点机制。
//
// 【方向由谁定】方向光的照射方向 = 光源实体 TransformComponent 的朝向？
//   本阶段先用组件自带的 direction 字段（简单直观，编辑器直接可调）；
//   以后可改成读 Transform 旋转（更符合 Godot"转动光源节点改方向"）。
//
// 【type】第 1 阶只实现方向光；点光源/聚光灯字段预留，第 2 阶多光源时启用：
//   0 = Directional 方向光（太阳，无位置只有方向，不衰减）
//   1 = Point       点光源（灯泡，有位置，随距离衰减）—— 第 2 阶
//   2 = Spot        聚光灯（手电，有位置+方向+锥角）—— 第 2 阶
//
// REFLECT 标记的字段由 reflect_gen 自动生成反射注册（编辑器/序列化零改动）。
// ============================================================
struct LightComponent
{
    REFLECT int type = 0; // 0=方向光 1=点光 2=聚光（本阶段只用 0）

    // 方向光用：照射方向（会归一化；点光源忽略此字段用实体位置）
    REFLECT glm::vec3 direction{-0.3f, -0.7f, -0.5f};

    // 颜色分量（对标 Blinn-Phong 三项；调色就调这里）
    REFLECT glm::vec3 ambient{0.2f, 0.2f, 0.2f};  // 环境光（照亮暗部）
    REFLECT glm::vec3 diffuse{0.8f, 0.8f, 0.8f};  // 漫反射（主色调）
    REFLECT glm::vec3 specular{1.0f, 1.0f, 1.0f}; // 高光

    REFLECT float intensity = 1.0f; // 强度倍率（整体明暗，diffuse/specular 乘它）

    // 点光源衰减参数（第 2 阶用；方向光忽略）
    REFLECT float range = 10.0f; // 有效半径

    // 聚光灯锥角（度，半角）——聚光灯(type=2)用；越小光锥越窄
    REFLECT float spotAngle = 30.0f;
};

#endif
