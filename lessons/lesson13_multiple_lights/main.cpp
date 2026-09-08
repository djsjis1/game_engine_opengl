// ============================================================
// Lesson 13: 多光源 (Multiple Lights)
// ============================================================
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
    widget.resize(800, 600);
    widget.setWindowTitle("Lesson 13 - Multiple Lights (方向光 + 4个点光源)");
    widget.show();
    return app.exec();
}
