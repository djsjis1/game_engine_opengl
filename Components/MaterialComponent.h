#ifndef MATERIALCOMPONENT_H
#define MATERIALCOMPONENT_H

#include <glm/glm.hpp>
#include <cstdint>
#include "Components/Reflect.h"

// ============================================================
// MaterialComponent — PBR 材质（金属度/粗糙度工作流）
//
// 对标 Godot StandardMaterial3D / UE 标准材质，现代引擎统一模型：
//   albedo    基础色（非金属=漫反射色，金属=反射色调）
//   metallic  金属度 0~1：0=塑料/木头/石头  1=金属
//   roughness 粗糙度 0~1：0=镜面高光锐利  1=完全粗糙无高光
//   ao        环境光遮蔽 0~1：压暗缝隙处的环境光（1=不遮蔽）
//   albedoTex 基础色贴图（可选，相对资源根路径）：木纹/石纹等——
//             有贴图时 shader 用 贴图×albedo（albedo 当染色系数）
//
// 【为什么换掉 Blinn-Phong】旧模型 ambient/diffuse/specular/shininess
// 四个参数互相独立、没有物理约束，调不出真实质感；PBR 两个直觉滑杆
// （金属度/粗糙度）即可覆盖绝大多数材质，且能量守恒（高光和漫反射
// 自动此消彼长）。shader 端为 Cook-Torrance BRDF（default.frag）。
// ============================================================
struct MaterialComponent
{
    REFLECT glm::vec3 albedo{0.8f, 0.8f, 0.8f}; // 基础色（有贴图时当染色系数）
    REFLECT float metallic = 0.0f;              // 金属度
    REFLECT float roughness = 0.5f;             // 粗糙度
    REFLECT float ao = 1.0f;                    // 环境光遮蔽

    char albedoTex[128] = {}; // 基础色贴图相对路径（空=纯色）；序列化手工处理
    uint32_t texHandle = 0;   // 运行时纹理句柄（RenderWidget 惰性 acquire，不序列化）
};

#endif
