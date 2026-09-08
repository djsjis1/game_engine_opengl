#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include "shader.h"

// Lesson 17: 阴影映射 - 方向光投射阴影到地面和立方体上
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
    void renderDepthMap(); // 第一步：从光源视角渲染深度图
    void renderScene();    // 第二步：正常渲染 + 阴影计算
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_planeVAO = 0, m_planeVBO = 0;
    GLuint m_lightVAO = 0, m_lightVBO = 0;
    GLuint m_depthFBO = 0, m_depthTex = 0;
    Shader m_shadowShader; // 深度图渲染
    Shader m_sceneShader;  // 主场景 + 阴影
    Shader m_lightShader;  // 光源位置指示
    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = -30, m_pitch = 25, m_dist = 12;
    QPoint m_lastMouse;
    bool m_rotating = false;
    const int SHADOW_SIZE = 1024; // 阴影贴图分辨率
};
#endif
