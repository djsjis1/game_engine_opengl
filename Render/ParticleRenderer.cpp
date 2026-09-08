#include "Render/ParticleRenderer.h"
#include "Components/ParticleComponent.h"
#include "Components/TransformComponent.h"
#include <QDebug>

void ParticleRenderer::init(const std::string &shaderDir)
{
    initializeOpenGLFunctions();
    if (!m_shader.load(shaderDir + "particle.vert", shaderDir + "particle.frag"))
        qWarning() << "Particle shader load failed";

    // 共享单位 quad（triangle strip 4 顶点）
    const float quad[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quadVbo);
    glGenBuffers(1, &m_instVbo);
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // aCorner
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // 实例属性：iPos(3) + iSize(1) + iColor(4) = 8 float/粒子，divisor=1
    glBindBuffer(GL_ARRAY_BUFFER, m_instVbo);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(1); // iPos
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2); // iSize
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3); // iColor
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    glBindVertexArray(0);
}

void ParticleRenderer::cleanup()
{
    if (m_vao)
    {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quadVbo);
        glDeleteBuffers(1, &m_instVbo);
    }
    m_shader.clear();
}

void ParticleRenderer::draw(flecs::world &world, const glm::mat4 &view, const glm::mat4 &proj)
{
    if (!m_shader.isValid() || !m_vao)
        return;

    // 相机右/上轴 = view 矩阵前两行（正交基转置），billboard 展开用
    glm::vec3 camRight(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camUp(view[0][1], view[1][1], view[2][1]);

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setVec3("uCamRight", camRight.x, camRight.y, camRight.z);
    m_shader.setVec3("uCamUp", camUp.x, camUp.y, camUp.z);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE); // 深度只测不写：粒子间不互相剔除（半透明标准做法）
    glBindVertexArray(m_vao);

    world.each([&](flecs::entity, const TransformComponent &tf, const ParticleComponent &pc)
               {
        if (pc.pool.empty() || !tf.visibleInTree) return;

        // 打包实例数据：pos(3) size(1) color(4)，颜色/大小按寿命进度插值
        auto &buf = m_scratch;
        buf.clear();
        buf.reserve(pc.pool.size() * 8);
        for (const Particle &p : pc.pool)
        {
            float t = p.age / p.life; // 0→1 寿命进度
            glm::vec3 col = glm::mix(pc.colorStart, pc.colorEnd, t) * pc.intensity;
            float size = pc.sizeStart + (pc.sizeEnd - pc.sizeStart) * t;
            float alpha = 1.0f - t; // 渐隐
            buf.insert(buf.end(), {p.pos.x, p.pos.y, p.pos.z, size,
                                   col.r, col.g, col.b, alpha});
        }

        // 加法混合（火/光效，叠加发亮）vs 普通 alpha（烟/尘）
        if (pc.additive)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // orphan + 上传（GL_STREAM_DRAW：每帧全量重写的标准用法）
        glBindBuffer(GL_ARRAY_BUFFER, m_instVbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(buf.size() * sizeof(float)),
                     buf.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)pc.pool.size()); });

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 复原默认
    glDisable(GL_BLEND);
}
