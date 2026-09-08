#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>

// ============================================================
// Lesson 02: 你好，三角形
// 核心概念：
//   VBO (Vertex Buffer Object)    - 在GPU上存储顶点数据
//   VAO (Vertex Array Object)     - 存储顶点属性配置（指针布局）
//   着色器程序 (Shader Program)    - 运行在GPU上的小程序
//
// GPU渲染管线（简化）：
//   顶点数据 → [顶点着色器] → [光栅化] → [片段着色器] → 屏幕像素
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
    // 编译着色器程序（顶点着色器 + 片段着色器）
    GLuint createShaderProgram(const char *vertexSource, const char *fragmentSource);

    GLuint m_vao = 0;           // 顶点数组对象
    GLuint m_vbo = 0;           // 顶点缓冲对象
    GLuint m_shaderProgram = 0; // 着色器程序
};

#endif // OPENGLWIDGET_H
