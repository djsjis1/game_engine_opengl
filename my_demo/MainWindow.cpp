#include "MainWindow.h"
#include "RenderWidget.h"
#include "Engine.h"
#include "editor/EditorPanel.h"
#include "System/RotationSystem.h"
#include "System/ScriptSystem.h"
#include "System/MeshFactory.h"
#include "System/Input.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QApplication>
#include <QToolBar>
#include <QCoreApplication>
#include <QDir>

extern "C" void scriptSetInput(const bool keys[256], float dx, float dy);

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Mini Engine");
    resize(1200, 750);

    m_engine = new Engine;
    m_engine->addSystem(new RotationSystem);
    auto *scriptSys = new ScriptSystem;
    m_engine->addSystem(scriptSys);
    m_scriptSystem = scriptSys;

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    m_editor = new EditorPanel(central);
    m_editor->setWorld(&m_engine->world());
    m_editor->setMeshCreator([this](flecs::entity e)
                             {
        m_renderWidget->makeCurrent();
        MeshFactory f; MeshComponent m; f.createCube(m);
        e.set<MeshComponent>(m);
        m_renderWidget->doneCurrent(); });
    layout->addWidget(m_editor);

    m_renderWidget = new RenderWidget(central);
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setEditorCamera(&m_editorCam);
    m_renderWidget->setMouseTracking(true);
    layout->addWidget(m_renderWidget, 1);
    setCentralWidget(central);
    setMouseTracking(true);

    qApp->installEventFilter(this);

    // ★ 工具栏按钮
    auto *tb = addToolBar("Main");
    m_playAction = tb->addAction("▶ 运行");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::togglePlayMode);

    connect(m_editor, &EditorPanel::valueChanged,
            m_renderWidget, QOverload<>::of(&QWidget::update));

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
        float dt = m_elapsed.restart() / 1000.f;
        if (dt > 0.1f) dt = 0.016f;

        auto &in = Input::instance();

        if (m_isPlaying) {
            scriptSetInput(in.keyDown, in.mouseDX, in.mouseDY);
            m_engine->runFrame(dt);
        } else {
            m_editorCam.update(dt, float(width()) / std::max(height(), 1));
            m_engine->runFrame(dt);
        }
        in.endFrame();
        m_renderWidget->update(); });

    auto &w = m_engine->world();
    auto cam = w.entity();
    cam.set<TagComponent>({"MainCamera"});
    cam.set<TransformComponent>({{0, 3, 8}});
    cam.set<CameraComponent>({});
    m_scriptSystem->setEnabled(false); // 编辑模式默认关脚本
}

void MainWindow::startGameLoop()
{
    m_elapsed.start();
    m_timer->start(16);

    m_renderWidget->makeCurrent();
    auto cube = m_engine->world().entity();
    cube.set<TagComponent>({"ScriptCube"});
    cube.set<TransformComponent>({{0, 0, 2}, {0, 0, 0}, {1, 1, 1}});
    cube.set<MaterialComponent>({{0.2f, 0.2f, 0.8f}, {0.3f, 0.3f, 1}, {1, 1, 1}, 32});
    MeshComponent mesh;
    MeshFactory f;
    f.createCube(mesh);
    cube.set<MeshComponent>(mesh);
    ScriptComponent sc;
    strncpy(sc.filePath, "D:\\code\\.std\\opengl\\my_demo\\scripts\\demo.lua", sizeof(sc.filePath) - 1);
    cube.set<ScriptComponent>(sc);
    m_renderWidget->doneCurrent();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    m_timer->stop();
    m_renderWidget->cleanupGL();
    delete m_engine;
}

void MainWindow::togglePlayMode()
{
    m_isPlaying = !m_isPlaying;
    m_playAction->setText(m_isPlaying ? "■ 停止" : "▶ 运行");
    m_editor->setVisible(!m_isPlaying);
    m_scriptSystem->setEnabled(m_isPlaying);
    m_renderWidget->setShowGizmo(!m_isPlaying);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    auto &in = Input::instance();
    switch (event->type())
    {
    case QEvent::KeyPress:
        processKey(static_cast<QKeyEvent *>(event), true);
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Tab)
            togglePlayMode();
        if (m_isPlaying && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
            togglePlayMode();
        return false;
    case QEvent::KeyRelease:
        processKey(static_cast<QKeyEvent *>(event), false);
        return false;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        processMouse(static_cast<QMouseEvent *>(event));
        return false;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::processKey(QKeyEvent *event, bool pressed)
{
    auto &in = Input::instance();
    in.onKey(event->key(), pressed);
}

void MainWindow::processMouse(QMouseEvent *event)
{
    auto &in = Input::instance();
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::RightButton)
        in.onMousePress(true);
    else if (event->type() == QEvent::MouseButtonRelease && event->button() == Qt::RightButton)
        in.onMouseRelease(false);
    else if (event->type() == QEvent::MouseMove)
    {
        static QPoint last;
        if (!last.isNull())
        {
            QPoint d = event->globalPos() - last;
            in.onMouseMove((float)d.x(), (float)d.y());
        }
        last = event->globalPos();
    }
}
