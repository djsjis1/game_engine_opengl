#include "Render/PostProcessor.h"
#include <QOpenGLWidget>
#include <QDebug>

void PostProcessor::init(const std::string &shaderDir)
{
    initializeOpenGLFunctions();
    if (!m_postShader.load(shaderDir + "post.vert", shaderDir + "post.frag"))
        qWarning() << "Post shader load failed";
    if (!m_brightShader.load(shaderDir + "post.vert", shaderDir + "bright.frag"))
        qWarning() << "Bright shader load failed";
    if (!m_blurShader.load(shaderDir + "post.vert", shaderDir + "blur.frag"))
        qWarning() << "Blur shader load failed";
    glGenVertexArrays(1, &m_postVao); // 空 VAO（post.vert 用 gl_VertexID 生成顶点）
}

void PostProcessor::cleanup()
{
    if (m_sceneFbo)
    {
        glDeleteFramebuffers(1, &m_sceneFbo);
        glDeleteTextures(1, &m_sceneTex);
        glDeleteRenderbuffers(1, &m_sceneRbo);
    }
    if (m_bloomFbo[0])
    {
        glDeleteFramebuffers(2, m_bloomFbo);
        glDeleteTextures(2, m_bloomTex);
    }
    if (m_postVao)
        glDeleteVertexArrays(1, &m_postVao);
    m_postShader.clear();
    m_brightShader.clear();
    m_blurShader.clear();
}

void PostProcessor::ensureFbo(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    // 已存在且尺寸一致 → 复用
    if (m_sceneFbo && w == m_postW && h == m_postH)
        return;
    // 尺寸变了：先删旧的
    if (m_sceneFbo)
    {
        glDeleteFramebuffers(1, &m_sceneFbo);
        glDeleteTextures(1, &m_sceneTex);
        glDeleteRenderbuffers(1, &m_sceneRbo);
    }
    glGenFramebuffers(1, &m_sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFbo);
    // 颜色附件：RGBA16F（HDR——存储超过 1.0 的亮度，供色调映射）
    glGenTextures(1, &m_sceneTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneTex, 0);
    // 深度附件：RBO（只写不采样，用 RBO 比纹理省）
    glGenRenderbuffers(1, &m_sceneRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_sceneRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_sceneRbo);
    m_postW = w;
    m_postH = h;

    // Bloom ping-pong FBO：半分辨率（省 4 倍带宽，模糊后看不出差别）
    if (m_bloomFbo[0])
    {
        glDeleteFramebuffers(2, m_bloomFbo);
        glDeleteTextures(2, m_bloomTex);
    }
    m_bloomW = w / 2 > 1 ? w / 2 : 1;
    m_bloomH = h / 2 > 1 ? h / 2 : 1;
    glGenFramebuffers(2, m_bloomFbo);
    glGenTextures(2, m_bloomTex);
    for (int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i]);
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_bloomW, m_bloomH,
                     0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // 防模糊采到对边
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_bloomTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 全屏后处理：Bloom（亮度提取→高斯模糊）→ 色调映射/伽马/暗角 → 上屏
void PostProcessor::render(const EnvironmentComponent &env, int outW, int outH, GLuint outFbo)
{
    glDisable(GL_DEPTH_TEST); // 全屏贴图不需要深度
    glBindVertexArray(m_postVao);

    // ---- Bloom：亮度提取 + ping-pong 高斯模糊（半分辨率）----
    bool bloomOn = m_bloomFbo[0] && m_brightShader.isValid() && m_blurShader.isValid();
    if (bloomOn)
    {
        glViewport(0, 0, m_bloomW, m_bloomH);
        // ① 亮度提取：scene → bloom[0]（只留 HDR 溢出的高亮）
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
        m_brightShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneTex);
        m_brightShader.setInt("uScene", 0);
        m_brightShader.setFloat("uThreshold", 1.0f); // >1 才泛光（HDR 专属）
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ② 高斯模糊 4 趟（水平↔垂直 ping-pong：0→1→0→1）
        m_blurShader.use();
        m_blurShader.setInt("uImage", 0);
        for (int pass = 0; pass < 4; ++pass)
        {
            int src = pass % 2, dst = 1 - src;
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[dst]);
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[src]);
            m_blurShader.setInt("uHorizontal", pass % 2 == 0 ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

    // ---- 最终合成：场景 + Bloom → 色调映射 → 上屏 ----
    // 输出目标由调用方传入（QOpenGLWidget 的“屏幕”其实是它的默认 FBO）
    glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    glViewport(0, 0, outW, outH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_postShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTex);
    m_postShader.setInt("uScene", 0);
    m_postShader.setInt("uBloomOn", bloomOn ? 1 : 0);
    if (bloomOn)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[1]); // 最后一趟垂直模糊的结果
        m_postShader.setInt("uBloom", 1);
        glActiveTexture(GL_TEXTURE0);
    }
    m_postShader.setFloat("uExposure", env.exposure);
    m_postShader.setInt("uToneMap", 1);
    m_postShader.setFloat("uVignette", env.vignette); // 暗角由环境组件驱动
    glBindVertexArray(m_postVao);
    glDrawArrays(GL_TRIANGLES, 0, 3); // 一个覆盖全屏的大三角形
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
