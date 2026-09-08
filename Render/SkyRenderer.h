#ifndef SKYRENDERER_H
#define SKYRENDERER_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include "Render/Shader.h"
#include "Components/EnvironmentComponent.h"
#include <string>

// ============================================================
// SkyRenderer — 天空盒子系统（从 RenderWidget 拆出，单一职责）
// 渐变天空盒：一个单位立方体，渲染时去掉相机平移 → 无限远错觉
// ============================================================
class SkyRenderer : protected QOpenGLFunctions_3_3_Core
{
public:
    void init(const std::string &shaderDir);
    void cleanup();

    /// 画渐变天空（最先画，作为所有物体的背景）
    /// enabled=false 时跳过（纯色背景）
    void draw(const glm::mat4 &view, const glm::mat4 &proj,
              const EnvironmentComponent &env, bool enabled);

private:
    Shader m_shader;
    GLuint m_vao = 0, m_vbo = 0;
};

#endif
