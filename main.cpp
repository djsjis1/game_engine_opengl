#include <QApplication>
#include <QSurfaceFormat>

#include "MainWindow.h"
#include "Core/EngineLog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    MainWindow window;
    window.show();

    // 窗口显示后才启动游戏循环（此时 GL 上下文已就绪）
    window.startGameLoop();

    int rc = app.exec();
    shutdownEngineLog(); // spdlog 显式收尾（防静态析构顺序崩溃）
    return rc;
}
