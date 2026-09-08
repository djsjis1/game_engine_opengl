#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include "shader.h"

// Lesson 16: 实例化渲染 - 小行星带（1000颗）
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
    GLuint m_rockVAO = 0, m_rockVBO = 0, m_instanceVBO = 0;
    GLuint m_planetVAO = 0, m_planetVBO = 0;
    Shader m_shader;
    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = 0, m_pitch = -20, m_dist = 50;
    QPoint m_lastMouse;
    bool m_rotating = false;
    const int INSTANCE_COUNT = 1000;
};
#endif
