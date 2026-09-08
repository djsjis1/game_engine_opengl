#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>

// ============================================================
// Lesson 01: 创建OpenGL窗口
// 目标：理解QOpenGLWidget的三个核心虚函数
//       - initializeGL()  初始化时调用一次
//       - resizeGL()      窗口大小变化时调用
//       - paintGL()       每帧渲染时调用
//       - glClear()       清除颜色缓冲
// ============================================================
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    GLuint vao=0;// 顶点数组对象
    GLuint vbo=0;// 顶点缓冲对象
    GLuint ebo=0;// 索引缓冲对象
    GLuint shaderProgram=0;// 着色器程序

    void init(); // 初始化OpenGL资源


};

#endif // OPENGLWIDGET_H
