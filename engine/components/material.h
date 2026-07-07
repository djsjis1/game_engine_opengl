#ifndef ENGINE_COMPONENTS_MATERIAL_H
#define ENGINE_COMPONENTS_MATERIAL_H

#include <QVector3D>
#include <QString>
#include <QOpenGLFunctions_3_3_Core>

// ============================================================
// MaterialComponent: 材质属性（Phong 光照模型）
//
// 存储材质的光照参数和贴图引用。
// 对应 GLSL 中的 Material struct：
//   struct Material {
//       vec3 ambient;
//       vec3 diffuse;
//       vec3 specular;
//       float shininess;
//   };
//
// 纹理 ID 为 0 表示不使用纹理，直接用颜色值。
// ============================================================
struct MaterialComponent
{
    QVector3D ambient{0.2f, 0.2f, 0.2f};
    QVector3D diffuse{0.8f, 0.8f, 0.8f};
    QVector3D specular{1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;

    GLuint diffuseTexture = 0;
    GLuint specularTexture = 0;
    QString shaderName = "default";

    MaterialComponent() = default;
    MaterialComponent(QVector3D amb, QVector3D dif, QVector3D spe, float sh)
        : ambient(amb), diffuse(dif), specular(spe), shininess(sh) {}
    MaterialComponent(QVector3D amb, QVector3D dif, QVector3D spe, float sh,
                      GLuint difTex, GLuint speTex, QString name)
        : ambient(amb), diffuse(dif), specular(spe), shininess(sh),
          diffuseTexture(difTex), specularTexture(speTex), shaderName(name) {}
};

#endif // ENGINE_COMPONENTS_MATERIAL_H
