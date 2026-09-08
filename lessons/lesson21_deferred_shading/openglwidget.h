#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QVector>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include "shader.h"

// Lesson 21: 延迟渲染 - G-Buffer + 多光源光照Pass
struct PointLight
{
    QVector3D pos, color;
};

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
    void setupGBuffer();
    void renderGeometryPass(); // 第1步：G-Buffer填充
    void renderLightPass();    // 第2步：光照计算

    GLuint m_cubeVAO = 0, m_cubeVBO = 0;                        // 立方体
    GLuint m_quadVAO = 0, m_quadVBO = 0;                        // 全屏四边形
    GLuint m_sphereVAO = 0, m_sphereVBO = 0, m_sphereCount = 0; // 球体

    // G-Buffer FBO + 3个附件
    GLuint m_gbufferFBO = 0;
    GLuint m_gPosTex = 0, m_gNormalTex = 0, m_gAlbedoSpecTex = 0, m_gDepthRBO = 0;

    Shader m_gbufferShader; // 几何阶段
    Shader m_lightShader;   // 光照阶段

    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = -25, m_pitch = 20, m_dist = 6;
    QPoint m_lastMouse;
    bool m_rotating = false;
    QVector<PointLight> m_lights; // 多个点光源
    GLsizei m_fboW = 0, m_fboH = 0;
};
#endif
