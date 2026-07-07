#ifndef GAME_ENGINE_EDITOR_MAIN_WINDOW_H
#define GAME_ENGINE_EDITOR_MAIN_WINDOW_H

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QStatusBar>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>

#include "config.h"
#include "../engine/engine.h"
#include "viewport.h"
#include "hierarchy_panel.h"
#include "inspector_panel.h"
#include "asset_browser.h"
#include "export_dialog.h"

// ============================================================
// MainWindow: 编辑器主窗口
//
// 布局：
//   - Central:    Viewport (3D 视口)
//   - Left Dock:  HierarchyPanel (层级树)
//   - Right Dock: InspectorPanel (属性检查器)
//   - Bottom Dock: AssetBrowser (资产浏览器)
//   - StatusBar:  帧率 / 实体数
// ============================================================
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("Game Engine Editor — Qt + Flecs + OpenGL");

        // ---- Viewport (Central) ----
        m_viewport = new Viewport(&m_engine, this);
        setCentralWidget(m_viewport);

        // ---- Hierarchy (Left) ----
        m_hierarchy = new HierarchyPanel(&m_engine, this);
        auto *hierDock = new QDockWidget("Scene Hierarchy", this);
        hierDock->setWidget(m_hierarchy);
        hierDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        addDockWidget(Qt::LeftDockWidgetArea, hierDock);

        // ---- Inspector (Right) ----
        m_inspector = new InspectorPanel(&m_engine, this);
        auto *inspDock = new QDockWidget("Inspector", this);
        inspDock->setWidget(m_inspector);
        addDockWidget(Qt::RightDockWidgetArea, inspDock);

        // ---- Asset Browser (Bottom) ----
        m_assetBrowser = new AssetBrowser(&m_engine, this);
        auto *assetDock = new QDockWidget("Asset Browser", this);
        assetDock->setWidget(m_assetBrowser);
        addDockWidget(Qt::BottomDockWidgetArea, assetDock);

        // ---- 信号连接 ----
        connect(m_hierarchy, &HierarchyPanel::entitySelected,
                m_inspector, &InspectorPanel::onEntitySelected);
        connect(m_hierarchy, &HierarchyPanel::entitySelected,
                m_viewport, &Viewport::onEntitySelected);
        connect(m_inspector, &InspectorPanel::componentChanged,
                m_viewport, QOverload<>::of(&Viewport::update));

        // GL 上下文就绪后 → 初始化引擎 + 创建默认场景
        connect(m_viewport, &Viewport::glReady, this, [this]()
                {
            m_engine.init(SHADER_DIR);       // SHADER_DIR 为 CMake 注入的宏
            createDefaultScene();
            m_hierarchy->refresh(); });

        // ---- 菜单栏 ----
        setupMenus();

        // ---- 状态栏 + 帧循环 ----
        auto *fpsLabel = new QLabel("Ready");
        statusBar()->addPermanentWidget(fpsLabel);
        m_statusLabel = fpsLabel;

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]()
                {
            m_viewport->update();
            if (m_statusLabel)
            {
                int entityCount = static_cast<int>(m_engine.world().getAllEntities().size());
                m_statusLabel->setText(
                    QString("FPS: %1  Entities: %2")
                        .arg(m_viewport->fps(), 0, 'f', 1)
                        .arg(entityCount));
            } });
        timer->start(16);
    }

    ~MainWindow()
    {
        if (m_viewport)
            m_viewport->makeCurrent();
        m_engine.cleanupGL();
    }

private:
    void setupMenus()
    {
        // File
        auto *fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction("&New Scene", this, [this]() { /* TODO */ });
        fileMenu->addAction("&Open Scene...", this, [this]()
                            {
            auto path = QFileDialog::getOpenFileName(this, "Open Scene", "", "JSON (*.json)");
            if (!path.isEmpty())
            {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly))
                    m_engine.world().importFromJson(f.readAll().toStdString());
                m_hierarchy->refresh();
            } });
        fileMenu->addAction("&Save Scene...", this, [this]()
                            {
            auto path = QFileDialog::getSaveFileName(this, "Save Scene", "", "JSON (*.json)");
            if (!path.isEmpty())
            {
                QFile f(path);
                if (f.open(QIODevice::WriteOnly))
                    f.write(QByteArray::fromStdString(m_engine.world().exportToJson()));
            } });
        fileMenu->addSeparator();
        fileMenu->addAction("E&xit", this, &QWidget::close);

        // Edit
        auto *editMenu = menuBar()->addMenu("&Edit");
        editMenu->addAction("&Add Cube", this, [this]()
                            { addEntity("Cube"); });
        editMenu->addAction("Add &Light", this, [this]()
                            { addEntity("Light"); });
        editMenu->addAction("Add &Camera", this, [this]()
                            { addEntity("Camera"); });
        editMenu->addSeparator();
        editMenu->addAction("&Delete Selected", this, [this]()
                            { m_hierarchy->deleteSelected(); });

        // Export
        auto *exportMenu = menuBar()->addMenu("E&xport");
        exportMenu->addAction("Export as C++ &Project...", this, [this]()
                              {
            ExportDialog dlg(&m_engine, this);
            dlg.exec(); });

        // Help
        auto *helpMenu = menuBar()->addMenu("&Help");
        helpMenu->addAction("&About", this, []()
                            { QMessageBox::about(nullptr, "Game Engine Editor",
                                                 "3D Game Engine Editor\nQt + Flecs + OpenGL\nLearning Project"); });
    }

    void createDefaultScene()
    {
        auto &engine = m_engine;
        auto &factory = engine.meshFactory();

        // Ground
        auto ground = engine.createEntity("Ground");
        engine.setComponent(ground, TransformComponent{{0, -2, 0}, {0, 0, 0}, {10, 1, 10}});
        engine.setComponent(ground, factory.createCube(1.0f));
        engine.setComponent(ground, MaterialComponent{
                                        {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.5f}, {0.2f, 0.2f, 0.2f}, 8.0f});

        // Light
        auto light = engine.createEntity("PointLight");
        engine.setComponent(light, TransformComponent{{2, 3, 0}, {0, 0, 0}, {1, 1, 1}});
        engine.setComponent(light, LightComponent{
                                       LightType::Point,
                                       {0.2f, 0.2f, 0.2f},
                                       {0.8f, 0.8f, 0.8f},
                                       {1, 1, 1},
                                       1.0f,
                                       0.09f,
                                       0.032f});

        // Light marker (白 cube, 用 light_cube shader)
        auto marker = engine.createEntity("LightMarker");
        engine.setComponent(marker, TransformComponent{{2, 3, 0}, {0, 0, 0}, {1, 1, 1}});
        engine.setComponent(marker, factory.createCube(0.15f));
        {
            MaterialComponent mat;
            mat.ambient = {1, 1, 1};
            mat.diffuse = {1, 1, 1};
            mat.specular = {1, 1, 1};
            mat.shininess = 1.0f;
            strncpy(mat.shaderName, "light_cube", 63);
            engine.setComponent(marker, mat);
        }

        // Cube
        auto cube = engine.createEntity("Cube1");
        engine.setComponent(cube, TransformComponent{{0, 0.5f, 0}, {0, 0, 0}, {1, 1, 1}});
        engine.setComponent(cube, factory.createCube(0.8f));
        engine.setComponent(cube, MaterialComponent{
                                      {0.1f, 0.1f, 0.1f}, {0.8f, 0.3f, 0.3f}, {0.5f, 0.5f, 0.5f}, 32.0f});

        // Camera
        auto cam = engine.createEntity("MainCamera");
        engine.setComponent(cam, TransformComponent{{0, 2, 8}, {0, 0, 0}, {1, 1, 1}});
        engine.setComponent(cam, CameraComponent{
                                     {0, 2, 8}, {0, 0, -1}, {0, 1, 0}, 45.0f, 0.1f, 100.0f});
        m_viewport->setCameraEntity(cam);

        m_hierarchy->refresh();
    }

    void addEntity(const QString &type)
    {
        auto &engine = m_engine;
        auto e = engine.createEntity(type.toStdString());
        engine.setComponent(e, TransformComponent{});

        if (type == "Cube")
        {
            engine.setComponent(e, engine.meshFactory().createCube(1.0f));
            engine.setComponent(e, MaterialComponent{});
        }
        else if (type == "Light")
        {
            engine.setComponent(e, LightComponent{});
        }
        else if (type == "Camera")
        {
            engine.setComponent(e, CameraComponent{});
        }

        m_hierarchy->refresh();
    }

    Engine m_engine;
    Viewport *m_viewport = nullptr;
    HierarchyPanel *m_hierarchy = nullptr;
    InspectorPanel *m_inspector = nullptr;
    AssetBrowser *m_assetBrowser = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // GAME_ENGINE_EDITOR_MAIN_WINDOW_H
