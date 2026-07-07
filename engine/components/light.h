#ifndef ENGINE_COMPONENTS_LIGHT_H
#define ENGINE_COMPONENTS_LIGHT_H

#include <QVector3D>

// ============================================================
// LightComponent: 光源数据
//
// 支持三种光源类型（对应 lesson12_light_casters）：
//   - Directional: 平行光（如太阳），无位置衰减
//   - Point:       点光源（如灯泡），距离衰减
//   - Spot:        聚光灯（如手电筒），锥形 + 距离衰减
//
// 对应 GLSL 中的 Light struct：
//   struct Light {
//       int type;
//       vec3 position;
//       vec3 direction;
//       vec3 ambient;
//       vec3 diffuse;
//       vec3 specular;
//       float constant;
//       float linear;
//       float quadratic;
//       float cutOff;
//       float outerCutOff;
//   };
// ============================================================

// 光源类型枚举
enum class LightType
{
    Directional = 0, // 方向光
    Point = 1,       // 点光源
    Spot = 2         // 聚光灯
};

struct LightComponent
{
    LightType type = LightType::Point;

    QVector3D ambient{0.2f, 0.2f, 0.2f};
    QVector3D diffuse{0.5f, 0.5f, 0.5f};
    QVector3D specular{1.0f, 1.0f, 1.0f};

    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    float cutOff = 0.91f;
    float outerCutOff = 0.82f;

    LightComponent() = default;
    LightComponent(LightType tp, QVector3D amb, QVector3D dif, QVector3D spe,
                   float c, float l, float q, float co = 0.91f, float oc = 0.82f)
        : type(tp), ambient(amb), diffuse(dif), specular(spe),
          constant(c), linear(l), quadratic(q), cutOff(co), outerCutOff(oc) {}
};

#endif // ENGINE_COMPONENTS_LIGHT_H
