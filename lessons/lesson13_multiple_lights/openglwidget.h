#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include "shader.h"

// ============================================================
// Lesson 13: 多光源 (Multiple Lights)
//   1 个方向光 + 4 个点光源（颜色各异，绕场景旋转）
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
    QVector<QVector3D> m_pointLightPositions; // 点光源位置
    QVector<QVector3D> m_pointLightColors;    // 点光源颜色
    QVector<QVector3D> m_cubePositions;
};
#endif
