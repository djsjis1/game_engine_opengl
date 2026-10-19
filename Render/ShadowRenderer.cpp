#include "Render/ShadowRenderer.h"
#include "Core/TestFramework.h"
#include "Components/LightComponent.h"
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include <cmath>
#include <QDebug>

void ShadowRenderer::init(const std::string &shaderDir)
{
    initializeOpenGLFunctions();
    if (!m_depthShader.load(shaderDir + "depth.vert", shaderDir + "depth.frag"))
        qWarning() << "Depth shader load failed";
    if (!m_pointDepthShader.load(shaderDir + "depth_point.vert", shaderDir + "depth_point.frag"))
        qWarning() << "Point depth shader load failed";

    // ============================================================
    // 方向光/聚光深度图：单通道 depth，边缘白色(1.0=最远)防越界误判阴影
    // ============================================================
    glGenFramebuffers(1, &m_shadowFbo);
    glGenTextures(1, &m_shadowTex);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kDirSpotSize, kDirSpotSize,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex, 0);
    glDrawBuffer(GL_NONE); // 无颜色附件（只写深度）
    glReadBuffer(GL_NONE);
    m_shadowReady = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ============================================================
    // 点光阴影 cubemap（对标 Godot OmniLight3D 的 Shadow）
    // ============================================================
    glGenFramebuffers(1, &m_pointShadowFbo);
    glGenTextures(1, &m_pointShadowCube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_pointShadowCube);
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     kPointSize, kPointSize, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, m_pointShadowFbo);
    // 先附 +X 面占位验证完整性（渲染时逐面重新附加）
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_pointShadowCube, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    m_pointShadowReady =
        (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowRenderer::cleanup()
{
    if (m_shadowFbo)
    {
        glDeleteFramebuffers(1, &m_shadowFbo);
        glDeleteTextures(1, &m_shadowTex);
    }
    if (m_pointShadowFbo)
    {
        glDeleteFramebuffers(1, &m_pointShadowFbo);
        glDeleteTextures(1, &m_pointShadowCube);
    }
    m_depthShader.clear();
    m_pointDepthShader.clear();
}

// 返回光空间矩阵（proj*view）；无可投影光源返回单位阵（禁用阴影）。
// 方向光优先（正交投影）；没有方向光时用第一盏聚光灯（透视投影，
// 手电筒/舞台光效果）——同一张深度图和同一个 calcShadow 两者通用，
// 差别只在投影矩阵。outType/outPos 告诉主 pass "谁在投影"。
glm::mat4 ShadowRenderer::renderDirSpot(flecs::world &world, const RenderList &renderList,
                                        int &outType, glm::vec3 &outPos)
{
    outType = -1;
    if (!m_shadowReady || !m_depthShader.isValid())
        return glm::mat4(1);

    // 先找方向光（优先投影权）
    glm::vec3 dir(0);
    bool hasDir = false;
    world.each([&](flecs::entity, const LightComponent &lc)
               {
        if (!hasDir && lc.type == 0) { dir = glm::normalize(lc.direction); hasDir = true; } });

    glm::mat4 lightSpace(1);
    if (hasDir)
    {
        // 方向光：正交投影，从光的反方向"俯瞰"场景中心（覆盖 ±25）
        glm::vec3 center(0, 0, 0);
        glm::vec3 lightPos = center - dir * 30.0f;
        glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0, 1, 0));
        glm::mat4 lightProj = glm::ortho(-25.f, 25.f, -25.f, 25.f, 1.f, 80.f);
        lightSpace = lightProj * lightView;
        outType = 0;
    }
    else
    {
        // 聚光灯：透视投影（fov = 2×锥半角 + 余量），从灯位置照 direction
        bool hasSpot = false;
        glm::vec3 spotPos(0), spotDir(0, -1, 0);
        float spotAngle = 30.0f, range = 30.0f;
        world.each([&](flecs::entity, const LightComponent &lc,
                       const TransformComponent &tf)
                   {
            if (!hasSpot && lc.type == 2)
            {
                spotPos = glm::vec3(tf.worldMatrix[3]);
                spotDir = glm::normalize(lc.direction);
                spotAngle = lc.spotAngle;
                range = lc.range > 1.0f ? lc.range : 30.0f;
                hasSpot = true;
            } });
        if (!hasSpot)
            return glm::mat4(1); // 没有可投影的光

        // up 与照射方向接近平行时换轴（lookAt 退化保护）
        glm::vec3 up = std::fabs(glm::dot(spotDir, glm::vec3(0, 1, 0))) > 0.99f
                           ? glm::vec3(1, 0, 0)
                           : glm::vec3(0, 1, 0);
        glm::mat4 lightView = glm::lookAt(spotPos, spotPos + spotDir, up);
        glm::mat4 lightProj = glm::perspective(
            glm::radians(spotAngle * 2.0f + 10.0f), 1.0f, 0.3f, range);
        lightSpace = lightProj * lightView;
        outType = 2;
        outPos = spotPos;
    }

    // 渲染深度：切到 shadow FBO，只画各实体网格（无材质/光照）
    EngineTest::countShadowPass();
    glViewport(0, 0, kDirSpotSize, kDirSpotSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    m_depthShader.use();
    m_depthShader.setMat4("uLightSpaceMatrix", lightSpace);
    const RenderFrustum frustum = RenderFrustum::fromViewProjection(lightSpace);
    for (const RenderItem &item : renderList.items())
    {
        if (!frustum.intersects(item.worldBounds)) continue;
        m_depthShader.setMat4("uModel", item.model);
        const GpuMesh &gm = item.mesh;
        glBindVertexArray(gm.vao);
        if (gm.hasIndices) glDrawElements(gm.primitiveType, gm.indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(gm.primitiveType, 0, gm.vertexCount);
    }
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return lightSpace;
}

// 首个点光的 6 面深度渲染。返回 false = 场景无点光（主 pass 关点光阴影）
bool ShadowRenderer::renderPoint(flecs::world &world, const RenderList &renderList,
                                 glm::vec3 &outPos, float &outFar)
{
    if (!m_pointShadowReady || !m_pointDepthShader.isValid())
        return false;

    // 找第一个点光（type==1），位置取实体 Transform
    bool found = false;
    glm::vec3 lightPos(0);
    float range = 20.0f;
    world.each([&](flecs::entity, const LightComponent &lc, const TransformComponent &tf)
               {
        if (!found && lc.type == 1)
        {
            lightPos = glm::vec3(tf.worldMatrix[3]);
            range = lc.range > 1.0f ? lc.range : 20.0f;
            found = true;
        } });
    if (!found)
        return false;

    // 6 个面的 view 矩阵（cubemap 面朝向约定：+X -X +Y -Y +Z -Z）
    const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, range);

    glViewport(0, 0, kPointSize, kPointSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pointShadowFbo);
    m_pointDepthShader.use();
    m_pointDepthShader.setVec3("uLightPos", lightPos.x, lightPos.y, lightPos.z);
    m_pointDepthShader.setFloat("uFarPlane", range);

    for (int face = 0; face < 6; ++face)
    {
        EngineTest::countShadowPass();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               m_pointShadowCube, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glm::mat4 view = glm::lookAt(lightPos, lightPos + dirs[face], ups[face]);
        const glm::mat4 faceSpace = proj * view;
        m_pointDepthShader.setMat4("uLightSpaceMatrix", faceSpace);
        const RenderFrustum frustum = RenderFrustum::fromViewProjection(faceSpace);
        for (const RenderItem &item : renderList.items())
        {
            if (!frustum.intersects(item.worldBounds)) continue;
            m_pointDepthShader.setMat4("uModel", item.model);
            const GpuMesh &gm = item.mesh;
            glBindVertexArray(gm.vao);
            if (gm.hasIndices) glDrawElements(gm.primitiveType, gm.indexCount, GL_UNSIGNED_INT, nullptr);
            else glDrawArrays(gm.primitiveType, 0, gm.vertexCount);
        }
        glBindVertexArray(0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    outPos = lightPos;
    outFar = range;
    return true;
}
