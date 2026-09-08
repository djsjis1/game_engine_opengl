#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include "shader.h"

// Lesson 18: 法线贴图 - 用贴图伪造表面凹凸细节（不增加顶点）
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
    GLuint generateBrickNormalMap(int w, int h); // 程序化生成砖墙法线贴图
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    GLuint m_lightVAO = 0, m_lightVBO = 0;
    GLuint m_normalTex = 0;
    Shader m_normalShader, m_lightShader;
    QTimer *m_timer = nullptr;
    float m_angle = 0, m_yaw = 0, m_pitch = 10, m_dist = 5;
    QPoint m_lastMouse;
    bool m_rotating = false;
    bool m_showNormalMap = false; // 按N切换显示法线贴图
};
#endif
