// ============================================================
// Lesson 12: 投光物 (Light Casters)
//   按 1/2/3 切换光源类型
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
    widget.setWindowTitle("Lesson 12 - Light Casters (方向光/点光源/聚光灯)  1/2/3 切换");
    widget.show();
    return app.exec();
}
