#include "openglwidget.h"
#include <QDebug>

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

void OpenGLWidget::initializeGL()
{
    // 【知识点】initializeOpenGLFunctions() 必须首先调用，
    // 它会初始化 QOpenGLFunctions_3_3_Core 提供的函数指针
    initializeOpenGLFunctions();

    // 【知识点】glClearColor(r, g, b, a) 设置清屏颜色
    // 参数范围 0.0 ~ 1.0，分别对应红、绿、蓝、透明度
    // 这里设置为深蓝灰色
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    // 打印OpenGL版本信息（调试用）
    qDebug() << "OpenGL Version:" << (const char *)glGetString(GL_VERSION);
    qDebug() << "GLSL Version:" << (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    // 【知识点】glViewport(x, y, width, height) 设置渲染区域
    // 通常设置为整个窗口的大小
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    // 【知识点】glClear(mask) 清除指定的缓冲区
    // GL_COLOR_BUFFER_BIT  - 颜色缓冲区
    // GL_DEPTH_BUFFER_BIT  - 深度缓冲区
    // GL_STENCIL_BUFFER_BIT - 模板缓冲区
    // 用 initializeGL 中设置的颜色填充整个窗口
    glClear(GL_COLOR_BUFFER_BIT);
}
