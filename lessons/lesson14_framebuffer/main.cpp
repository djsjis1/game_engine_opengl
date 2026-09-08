// Lesson 14: 帧缓冲 + 后期处理特效
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
    w.setWindowTitle("Lesson 14 - Framebuffer + Post-Processing  1原图 2反相 3灰度 4锐化 5模糊 6边缘");
    w.show();
    return app.exec();
}
