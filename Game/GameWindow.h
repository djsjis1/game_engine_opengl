#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <memory>

class RenderWidget;
class Engine;
class EngineHost;

// ============================================================
// GameWindow — 游戏运行时窗口（my_game.exe 的主窗口）
//
// 【与编辑器 MainWindow 的区别】这是给"玩家"的，不是给"开发者"的：
//   没有场景树/Inspector/工具栏/脚本编辑器 —— 只有一个全窗口视口
//   启动即进 Play 状态直接跑游戏（没有编辑模式的概念）
//   不快照/还原场景（游戏退出就退出，没有"回编辑"）
//
// 【启动流程】读 game.json{main_scene} → 加载场景 → 路径重定位
//   （导出包里资源跟 exe 走）→ setPlay → 游戏循环
// ============================================================
class GameWindow : public QMainWindow
{
public:
    explicit GameWindow(QWidget *parent = nullptr);
    ~GameWindow() override;
    void startGameLoop();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void loadGame();                        // 读配置 + 加载主场景 + 路径重定位
    bool loadScene(const QString &relPath); // 加载指定场景（含 rebase/autoplay，切关用）
    void processMouse(QMouseEvent *event);  // 鼠标事件 → Input

    RenderWidget *m_renderWidget = nullptr;
    std::unique_ptr<EngineHost> m_host;
    Engine *m_engine = nullptr;
};

#endif
