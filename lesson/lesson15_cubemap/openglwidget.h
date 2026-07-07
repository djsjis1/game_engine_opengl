#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include "shader.h"

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
    void keyPressEvent(QKeyEvent *e) override;

private:
    GLuint loadCubemap(); // 程序化生成 6 面渐变天空
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_skyboxVAO = 0, m_skyboxVBO = 0;
    GLuint m_cubemapTex = 0;
    Shader m_cubeShader, m_skyboxShader;
    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = 0, m_pitch = -20;
    QPoint m_lastMouse;
    bool m_rotating = false;
};
#endif
