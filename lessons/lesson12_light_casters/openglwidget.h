#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include "shader.h"

// ============================================================
// Lesson 12: 投光物 (Light Casters)
//   三种光源类型，按 1/2/3 切换：
//   1. 方向光 (Directional) - 平行光，无位置只有方向
//   2. 点光源 (Point)       - 点光源 + 衰减
//   3. 聚光灯 (Spot)        - 点光源 + 方向 + 切角
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
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_lightVAO = 0, m_lightVBO = 0;
    Shader m_lightingShader, m_lightCubeShader;
    QTimer *m_timer = nullptr;
    float m_time = 0.0f;
    int m_lightType = 0; // 0=Directional, 1=Point, 2=Spot
    QVector<QVector3D> m_cubePositions;
};
#endif
