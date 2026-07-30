// ============================================================
// main_game.cpp — 游戏运行时入口（my_game.exe）
//
// 与编辑器 main.cpp 的区别：不建 MainWindow（没有编辑器 UI），
// 只建 GameWindow：加载场景 → 直接进 Play → 玩家开玩。
// 这就是"导出的游戏"——用户拿到的是游戏，不是引擎框架。
// ============================================================
#include "Game/GameWindow.h"
#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    // OpenGL 3.3 Core（与编辑器一致）
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    GameWindow w;
    w.show();
    w.startGameLoop();
    return app.exec();
}
