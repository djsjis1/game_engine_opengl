// Lesson 21: 延迟渲染 Deferred Shading
#include <QApplication>
#include <QSurfaceFormat>
#include "openglwidget.h"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);
    OpenGLWidget w;
    w.resize(800, 600);
    w.setWindowTitle("Lesson 21 - Deferred Shading  10个移动光源 | 鼠标旋转");
    w.show();
    return app.exec();
}
