#include "Render/SkyRenderer.h"
#include <QDebug>

void SkyRenderer::init(const std::string &shaderDir)
{
    initializeOpenGLFunctions();
    if (!m_shader.load(shaderDir + "sky.vert", shaderDir + "sky.frag"))
        qWarning() << "Sky shader load failed";

    // 单位立方体 36 顶点（6 面 × 2 三角形 × 3 顶点），只需位置
    const float verts[] = {
        -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
        -1, -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1,
        1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1,
        -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1, 1,
        -1, 1, -1, 1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1, 1, -1, 1, -1,
        -1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SkyRenderer::cleanup()
{
    if (m_vao)
    {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
    }
    m_shader.clear();
}

void SkyRenderer::draw(const glm::mat4 &view, const glm::mat4 &proj,
                       const EnvironmentComponent &env, bool enabled)
{
    if (!enabled || !env.skyEnabled || !m_shader.isValid())
        return;
    // 深度函数改 LEQUAL：天空深度恒为 1.0，要能通过"≤"测试才画得出
    glDepthFunc(GL_LEQUAL);
    m_shader.use();
    // 去掉 view 的平移分量（取左上 3x3 旋转）→ 天空跟随相机
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    m_shader.setMat4("uView", skyView);
    m_shader.setMat4("uProjection", proj);
    // 渐变天空色由环境组件驱动（编辑器可实时调，对标 Godot ProceduralSky）
    m_shader.setVec3("uHorizonColor", env.horizonColor.x, env.horizonColor.y, env.horizonColor.z);
    m_shader.setVec3("uZenithColor", env.zenithColor.x, env.zenithColor.y, env.zenithColor.z);
    m_shader.setVec3("uGroundColor", env.groundColor.x, env.groundColor.y, env.groundColor.z);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS); // 恢复默认，不影响后续物体渲染
}
