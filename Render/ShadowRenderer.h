#ifndef SHADOWRENDERER_H
#define SHADOWRENDERER_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#include "Render/Shader.h"
#include "Render/RenderList.h"
#include <string>

// ============================================================
// ShadowRenderer — 阴影子系统（从 RenderWidget 拆出，单一职责）
//
// 职责：从光源视角渲染深度图。谁用这些深度、怎么采样由主 pass 决定。
//   ├─ 方向光/聚光 → 2D 深度图 (2048×2048)
//   └─ 点光 → Cubemap 6 面深度 (512×512×6，线性距离存储)
//
// 生命周期：init() 必须在 GL 上下文内调用；cleanup() 在上下文销毁前。
// ============================================================
class ShadowRenderer : protected QOpenGLFunctions_3_3_Core
{
public:
    /// 加载 shader + 创建 FBO（需 GL 上下文）
    void init(const std::string &shaderDir);
    /// 释放全部 GL 资源（需 GL 上下文）
    void cleanup();

    /// 方向光/聚光 → 2D 深度图。返回光空间矩阵（单位阵 = 无可投影光）
    /// outType：0=方向光 2=聚光（-1=无）；outPos：聚光位置
    glm::mat4 renderDirSpot(flecs::world &world, const RenderList &renderList,
                            int &outType, glm::vec3 &outPos);

    /// 首个点光 → cubemap 6 面深度。返回 false = 场景无点光
    /// outPos/outFar：主 pass 采样用（光源位置 + 深度范围）
    bool renderPoint(flecs::world &world, const RenderList &renderList,
                     glm::vec3 &outPos, float &outFar);

    // ---- 主 pass 绑定用（纹理单元由调用方管理）----
    GLuint dirSpotTexture() const { return m_shadowTex; }
    GLuint pointCube() const { return m_pointShadowCube; }
    bool dirSpotReady() const { return m_shadowReady; }
    bool pointReady() const { return m_pointShadowReady; }

private:
    static constexpr int kDirSpotSize = 2048; // 方向光/聚光深度图分辨率
    static constexpr int kPointSize = 512;    // 点光 cubemap 每面分辨率

    Shader m_depthShader;      // 方向光/聚光：深度渲染
    Shader m_pointDepthShader; // 点光：cubemap 深度渲染
    GLuint m_shadowFbo = 0, m_shadowTex = 0;
    GLuint m_pointShadowFbo = 0, m_pointShadowCube = 0;
    bool m_shadowReady = false;
    bool m_pointShadowReady = false;
};

#endif
