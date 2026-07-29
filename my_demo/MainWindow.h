#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QAction>

class EditorPanel;
class RenderWidget;
class Engine;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void startGameLoop();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void processKey(QKeyEvent *event, bool pressed);
    void processMouse(QMouseEvent *event);
    void togglePlayMode();

    EditorPanel *m_editor = nullptr;
    RenderWidget *m_renderWidget = nullptr;
    Engine *m_engine = nullptr;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsed;
    QAction *m_playAction = nullptr;
};

#endif
