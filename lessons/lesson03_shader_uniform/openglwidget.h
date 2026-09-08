#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QTimer>
#include "shader.h"

// ============================================================
// Lesson 03: 着色器与Uniform
// 核心概念：
//   1. 顶点颜色属性 - 每个顶点可以有多种属性
//   2. 颜色插值 - 三角形内部颜色自动渐变
//   3. uniform 变量 - CPU向GPU传递全局数据
//   4. Shader工具类 - 从文件加载着色器
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

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader  m_shader;      // 使用Shader工具类代替原始GLuint
    QTimer* m_timer = nullptr;  // 定时器用于动态更新
    float   m_time = 0.0f;      // 累计时间（用于动画）
};

#endif // OPENGLWIDGET_H
