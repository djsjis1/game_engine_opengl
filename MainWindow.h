#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QAction>
#include <QVector>
#include <string>
#include <memory>

class EditorPanel;
class QShortcut;
class RenderWidget;
class Engine;
class EngineHost;
class ScriptEditor;
class QStackedWidget;
class QPlainTextEdit;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void startGameLoop();

    /// 打开脚本到内置编辑器并切到 Script 页（资源双击 .lua 调用）
    void openScriptInEditor(const QString &path);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void processKey(QKeyEvent *event, bool pressed);
    void processMouse(QMouseEvent *event);
    void processWheel(class QWheelEvent *event); // 滚轮事件 → Input::onWheel
    void togglePlayMode();
    void showViewport(); // 切到 3D 视口页
    void showScript();   // 切到脚本编辑器页

    EditorPanel *m_editor = nullptr;
    RenderWidget *m_renderWidget = nullptr;
    std::unique_ptr<EngineHost> m_host;
    Engine *m_engine = nullptr;
    QAction *m_playAction = nullptr;

    // 主屏切换（对标 Godot 顶部 [3D][Script]）
    QStackedWidget *m_centerStack = nullptr; // 中央区：叠放视口页/脚本页
    ScriptEditor *m_scriptEditor = nullptr;
    QPlainTextEdit *m_outputPanel = nullptr; // 底部"输出"面板（脚本错误，Godot Output 同款）
    QAction *m_viewAction = nullptr;         // [3D 视口] 切换按钮
    QAction *m_scriptAction = nullptr;       // [脚本] 切换按钮

    // 进 Play 时的场景路径（Play 中切过关 → 退出恢复备份时一并还原）
    std::string m_sceneBeforePlay;
};

#endif
