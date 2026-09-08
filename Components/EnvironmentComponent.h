#ifndef ENVIRONMENTCOMPONENT_H
#define ENVIRONMENTCOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"

// ============================================================
// EnvironmentComponent — 场景环境（对标 Godot WorldEnvironment）
//
// 【为什么需要】天空色/曝光/暗角/雾这些参数原本硬编码在 RenderWidget.cpp，
//   想做黄昏/雾天场景得改 C++ 重编译。做成组件后：环境参数变成"场景数据"，
//   编辑器 Inspector 实时调、每个场景各自氛围、随场景序列化——这正是
//   Godot WorldEnvironment 节点存在的意义。
//
// 【用法】场景里放一个实体挂本组件（场景级单例，RenderWidget 取第一个）。
//   无此组件时 RenderWidget 回退到这里的默认值（老场景观感不变）。
//
// REFLECT 字段由 reflect_gen 自动生成反射（编辑器/序列化零改动）。
// ============================================================
struct EnvironmentComponent
{
    // —— 天空盒渐变色（drawSky 用）——
    REFLECT glm::vec3 horizonColor{0.55f, 0.65f, 0.78f}; // 地平线（浅）
    REFLECT glm::vec3 zenithColor{0.18f, 0.32f, 0.55f};  // 天顶（深）
    REFLECT glm::vec3 groundColor{0.20f, 0.20f, 0.22f};  // 地平线以下
    REFLECT bool skyEnabled = true;                      // 关掉则纯色背景

    // —— 后处理（drawPostProcess 用）——
    REFLECT float exposure = 1.0f;  // 曝光倍率（越大越亮）
    REFLECT float vignette = 0.25f; // 暗角强度（0=关，越大边缘越暗）

    // —— 距离雾（default.frag 用；密度 0 = 关闭）——
    REFLECT glm::vec3 fogColor{0.6f, 0.65f, 0.7f}; // 雾色（远处物体渐变到此色）
    REFLECT float fogDensity = 0.0f;               // 指数雾密度，0 关闭，0.02~0.1 明显
};

#endif
