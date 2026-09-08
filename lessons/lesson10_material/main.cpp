#include <QApplication>
#include <QSurfaceFormat>
#include "openglwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    OpenGLWidget widget;
    widget.resize(1000, 500);
    widget.setWindowTitle("Lesson 10 - Material Properties");
    widget.show();
    return app.exec();
}
