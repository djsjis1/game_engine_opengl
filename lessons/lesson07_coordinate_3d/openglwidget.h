#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include "shader.h"

// ============================================================
// Lesson 07: 3D坐标系统
// 核心概念：
//   1. Model矩阵   - 物体在"世界"中的位置/旋转/缩放
//   2. View矩阵    - 摄像机的位置和朝向 (lookAt)
//   3. Projection矩阵 - 透视投影（近大远小）
//   4. 深度测试    - 正确处理前后遮挡关系
//   5. 顶点数据    - 完整的3D立方体顶点
//
// 变换流水线（从物体到屏幕）：
//   局部空间 → [Model] → 世界空间 → [View] → 观察空间
//   → [Projection] → 裁剪空间 → [透视除法] → NDC → 屏幕
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

    GLuint m_vao = 0, m_vbo = 0;
    GLuint m_texture = 0;
    Shader  m_shader;
    QTimer* m_timer = nullptr;
    float   m_time = 0.0f;
};

#endif
