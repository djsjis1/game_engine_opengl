#ifndef PARTICLERENDERER_H
#define PARTICLERENDERER_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#include "Render/Shader.h"
#include <string>
#include <vector>

// ============================================================
// ParticleRenderer — 粒子渲染子系统（从 RenderWidget 拆出，单一职责）
// 单位 quad + 每实例属性（pos/size/color），一发射器一次实例化绘制。
// billboard 展开在 particle.vert（相机轴）。
// ============================================================
class ParticleRenderer : protected QOpenGLFunctions_3_3_Core
{
public:
    void init(const std::string &shaderDir);
    void cleanup();

    /// 实例化画所有发射器的活粒子（不透明物体之后、后处理之前调用）
    void draw(flecs::world &world, const glm::mat4 &view, const glm::mat4 &proj);

private:
    Shader m_shader;
    GLuint m_vao = 0, m_quadVbo = 0, m_instVbo = 0;
    std::vector<float> m_scratch; // 实例数据暂存（每帧复用，防反复分配）
};

#endif
