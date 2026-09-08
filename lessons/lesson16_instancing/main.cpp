// Lesson 16: 实例化渲染 - 小行星带
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
    w.setWindowTitle("Lesson 16 - Instancing (1000颗小行星)  鼠标旋转/滚轮缩放");
    w.show();
    return app.exec();
}
