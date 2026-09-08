// ============================================================
// Lesson 01: 创建OpenGL窗口
// 知识点：
// 1. QSurfaceFormat 设置OpenGL版本和Profile
// 2. QOpenGLWidget 基础用法
// 3. initializeGL / resizeGL / paintGL 三个生命周期函数
// 4. glClearColor + glClear 清屏
// ============================================================

#include <QApplication>
#include <QSurfaceFormat>
#include "openglwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 【知识点】设置 OpenGL 版本格式
    // CoreProfile: 只使用核心特性，废弃的功能不可用（推荐）
    // CompatibilityProfile: 兼容模式，可以使用旧API
    QSurfaceFormat format;
    format.setVersion(3, 3);                        // OpenGL 3.3
    format.setProfile(QSurfaceFormat::CoreProfile); // 核心模式
    format.setDepthBufferSize(24);                  // 24位深度缓冲
    QSurfaceFormat::setDefaultFormat(format);

    OpenGLWidget widget;
    widget.resize(800, 600);
    widget.setWindowTitle("Lesson 01 - Hello OpenGL Window");
    widget.show();

    return app.exec();
}
