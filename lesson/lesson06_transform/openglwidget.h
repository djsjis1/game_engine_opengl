#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include "shader.h"

// ============================================================
// Lesson 06: 变换 (Transformations)
// 核心概念：
//   1. QMatrix4x4 - Qt内置的4x4矩阵，代替glm
//      - translate(dx, dy, dz)  平移
//      - rotate(angle, x, y, z) 旋转（角度制）
//      - scale(sx, sy, sz)      缩放
//   2. 变换顺序很重要！先缩放→再旋转→最后平移
//   3. 矩阵通过uniform传递给着色器
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
    QImage createCheckerImage(int size, QColor c1, QColor c2);
    GLuint createTexture(const QImage& image);

    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_texture = 0;
    Shader  m_shader;
    QTimer* m_timer = nullptr;
    float   m_time = 0.0f;
};

#endif
