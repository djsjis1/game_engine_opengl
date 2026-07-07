#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>

#include "System/CameraInput.h"

class EditorPanel;
class RenderWidget;
class Engine;
class CameraSystem;

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

    EditorPanel *m_editor = nullptr;
    RenderWidget *m_renderWidget = nullptr;
    Engine *m_engine = nullptr;
    CameraSystem *m_cameraSys = nullptr;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsed;
    CameraInput m_input;
};

#endif
