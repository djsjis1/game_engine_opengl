#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include "shader.h"

// Lesson 20: 几何着色器 - 在GPU上生成/修改图元
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
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    Shader m_explodeShader; // 炸裂效果(可退化为普通)
    Shader m_normalShader;  // 法线可视化
    QTimer *m_timer = nullptr;
    float m_time = 0, m_yaw = -30, m_pitch = 20, m_dist = 6;
    QPoint m_lastMouse;
    bool m_rotating = false;
    int m_mode = 0;               // 0=炸裂, 1=法线可视化, 2=普通
    float m_explodeFactor = 0.5f; // 炸裂强度
};
#endif
