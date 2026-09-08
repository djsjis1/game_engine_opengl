// Lesson 20: 几何着色器 Geometry Shader
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
    w.setWindowTitle("Lesson 20 - Geometry Shader  1=炸裂 2=法线 3=普通 | 滚轮=炸裂强度");
    w.show();
    return app.exec();
}
