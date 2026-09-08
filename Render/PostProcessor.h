#ifndef POSTPROCESSOR_H
#define POSTPROCESSOR_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include "Render/Shader.h"
#include "Components/EnvironmentComponent.h"

// ============================================================
// PostProcessor — 后处理子系统（从 RenderWidget 拆出，单一职责）
//
// 职责：HDR 场景 FBO + Bloom + 色调映射上屏。
//   ├─ ensureFbo(w,h)：窗口尺寸变化时重建场景 FBO（RGBA16F）+ Bloom 半分辨率
//   ├─ render()：Bloom（亮度提取→高斯模糊 ping-pong）→ ACES 色调映射 → 上屏
//
// 与 RenderWidget 的接口：
//   - sceneFbo()：主 pass 绑定到场景离屏 FBO（返回 0 = 后处理不可用）
//   - ready()：FBO 完整 + shader 就绪
// ============================================================
class PostProcessor : protected QOpenGLFunctions_3_3_Core
{
public:
    void init(const std::string &shaderDir);
    void cleanup();

    /// 确保场景 FBO + Bloom FBO 匹配窗口尺寸（尺寸没变则复用）
    void ensureFbo(int w, int h);

    /// 全屏后处理：Bloom → 色调映射/伽马/暗角 → 上屏
    /// env 驱动曝光/暗角参数（环境组件）
    /// outFbo：输出目标（RenderWidget 传 defaultFramebufferObject()）
    void render(const EnvironmentComponent &env, int outW, int outH, GLuint outFbo);

    GLuint sceneFbo() const { return m_sceneFbo; }
    bool ready() const { return m_sceneFbo != 0 && m_postShader.isValid(); }

private:
    Shader m_postShader;   // 后处理：色调映射/伽马/暗角 + Bloom 合成
    Shader m_brightShader; // Bloom①：亮度提取
    Shader m_blurShader;   // Bloom②：高斯模糊（ping-pong 两趟）

    GLuint m_sceneFbo = 0, m_sceneTex = 0, m_sceneRbo = 0; // 场景离屏 FBO
    GLuint m_bloomFbo[2] = {0, 0}, m_bloomTex[2] = {0, 0}; // Bloom ping-pong（半分辨率）
    int m_bloomW = 0, m_bloomH = 0;                        // bloom FBO 尺寸
    GLuint m_postVao = 0;                                  // 全屏三角形（空 VAO，shader 生成顶点）
    int m_postW = 0, m_postH = 0;                          // 场景 FBO 当前尺寸
};

#endif
