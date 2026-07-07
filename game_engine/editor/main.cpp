// Game Engine Editor — 入口
#include <QApplication>
#include <QSurfaceFormat>
#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    MainWindow window;
    window.setWindowTitle("Game Engine Editor");
    window.resize(1280, 800);
    window.show();

    return app.exec();
}
