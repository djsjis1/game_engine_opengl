#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include "shader.h"

// Lesson 19: 泛光 Bloom - HDR 渲染 + 高斯模糊高亮区域 + 合成
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void setupFBOs();
    void renderSceneToFBO();
    void blurPass(GLuint srcTex, GLuint dstFBO); // 一次高斯模糊
    void renderComposite();

    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    // HDR FBO: 两个颜色附件（纹理），一个 RBO 深度
    GLuint m_hdrFBO = 0, m_hdrTex[2] = {0, 0}, m_rboDepth = 0;
    // 乒乓模糊 FBO: 两个纹理交替读写
    GLuint m_pingFBO[2] = {0, 0}, m_pingTex[2] = {0, 0};

    Shader m_sceneShader;     // 场景渲染
    Shader m_blurShader;      // 高斯模糊
    Shader m_compositeShader; // Bloom 合成

    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = -20, m_pitch = 25, m_dist = 8;
    QPoint m_lastMouse;
    bool m_rotating = false;
    int m_bloomLevel = 1; // 按 1~5 切换泛光强度
    GLsizei m_fboW = 0, m_fboH = 0;
};
#endif
