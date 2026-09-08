// Lesson 19: 泛光 Bloom
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
    w.setWindowTitle("Lesson 19 - Bloom  按 1~5 调泛光强度 | 鼠标旋转");
    w.show();
    return app.exec();
}
