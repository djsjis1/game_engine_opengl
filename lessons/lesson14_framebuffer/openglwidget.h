#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include "shader.h"

// ============================================================
// Lesson 14: 帧缓冲 (Framebuffer) + 后期处理特效
//   渲染到离屏纹理，再全屏绘制并施加特效
//   按键切换：1=原图 2=反相 3=灰度 4=锐化 5=模糊 6=边缘检测
// ============================================================
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupFBO(int w, int h);
    void renderScene();          // 第一阶段：渲染场景到 FBO
    void renderPostProcessing(); // 第二阶段：屏幕四边形 + 特效

    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0; // 全屏四边形
    GLuint m_fbo = 0, m_fboTexture = 0, m_fboRBO = 0;
    Shader m_sceneShader; // 场景着色器
    Shader m_postShader;  // 后期处理着色器
    QTimer *m_timer = nullptr;
    float m_time = 0.0f;
    int m_effect = 0; // 0=原图,1=反相,2=灰度,3=锐化,4=模糊,5=边缘检测
    int m_fboW = 800, m_fboH = 600;
};
#endif
