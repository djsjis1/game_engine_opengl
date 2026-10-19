#include "GameWindow.h"
#include "Render/RenderWidget.h"
#include "Render/SceneSerializer.h"
#include "Render/SceneResourcePreloader.h"
#include "Core/Engine.h"
#include "Core/EngineHost.h"
#include "Core/GamePaths.h"
#include "Core/SceneRouter.h" // 运行时切场景（change_scene 帧末执行）
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"

#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <fstream>
#include <nlohmann/json.hpp>

GameWindow::GameWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Game");
    resize(1280, 720);

    // ====== 引擎 + 系统（与编辑器同一套 System，少了编辑器 UI）======
    m_host = std::make_unique<EngineHost>(this, ScriptSystem::HotReloadMode::Disabled);
    m_engine = &m_host->engine();

    // ====== 视口（唯一 UI：整个窗口就是游戏画面）======
    m_renderWidget = new RenderWidget(this);
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setShowGizmo(false); // 玩家不该看到编辑器网格
    m_renderWidget->setPlayMode(true);   // 独立运行时永远用游戏相机
    m_renderWidget->setMouseTracking(true);
    setCentralWidget(m_renderWidget);
    setMouseTracking(true);
    qApp->installEventFilter(this);

    // ====== 加载游戏内容 → 直接进 Play（玩家没有"编辑模式"）======
    loadGame();
    m_engine->setPlay();

    m_host->setFpsCallback([this](float fps)
    {
        setWindowTitle(QString("MyGame - FPS: %1 (%2 ms)")
                           .arg(fps, 0, 'f', 1)
                           .arg(1000.f / fps, 0, 'f', 2));
    });
    m_host->setFrameCallback([this]()
    {
        m_host->applyMouseCapture(m_renderWidget, true);
        m_renderWidget->update();
    });
    m_host->setSceneChangeCallback([this](const std::string &path)
    {
        loadScene(QString::fromStdString(path));
    });
}

GameWindow::~GameWindow()
{
    qApp->removeEventFilter(this);
    m_host->stop();
    if (m_renderWidget->context())
        m_renderWidget->cleanupGL();
    m_renderWidget->setWorld(nullptr);
    m_host.reset();
}

void GameWindow::startGameLoop()
{
    m_host->start();
}

// ============================================================
// 加载游戏：game.json 指定主场景；导出包模式下重定位资源路径
// ============================================================
void GameWindow::loadGame()
{
    // 1. 读配置 game.json：{"main_scene": "scenes/xxx.json"}
    QString mainScene;
    {
        std::ifstream f((GamePaths::root() + "game.json").toStdString());
        if (f.is_open())
        {
            try
            {
                nlohmann::json cfg = nlohmann::json::parse(f);
                mainScene = QString::fromStdString(
                    cfg.value("main_scene", std::string()));
            }
            catch (...)
            { /* 配置坏了走回退 */
            }
        }
    }
    // 回退：没配置就拿 scenes/ 里第一个场景（开发模式方便直接跑）
    if (mainScene.isEmpty())
    {
        QDir dir(GamePaths::root() + "scenes");
        auto files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
        if (!files.isEmpty())
            mainScene = "scenes/" + files.first();
    }
    if (mainScene.isEmpty())
    {
        QMessageBox::warning(this, "无法启动",
                             "找不到场景：请确认 game.json 或 scenes/ 目录存在");
        return;
    }
    if (!loadScene(mainScene))
        QMessageBox::warning(this, "加载失败", "场景文件无法解析：" + mainScene);
}

// ============================================================
// 加载指定场景（启动 + 运行时切关共用）：
//   load → 导出包路径重定位 → autoplay 重触发 → 记录当前场景
// 旧实体的清理全自动：物理体（PhysicsSystem 死实体回收）、
// 脚本环境（OnRemove observer）、Mesh 资源（OnRemove 引用计数）
// ============================================================
bool GameWindow::loadScene(const QString &relPath)
{
    QString scenePath = GamePaths::root() + relPath;
    if (!SceneSerializer::load(m_engine->world(), scenePath.toStdString()))
        return false;
    SceneRouter::instance().setCurrent(relPath.toStdString());

    // 路径重定位（仅导出包）：场景存的开发机绝对路径 → exe 旁 assets/
    if (GamePaths::isPackaged())
    {
        auto fix = [](char *dst, size_t cap)
        {
            QString r = GamePaths::rebase(QString::fromUtf8(dst));
            strncpy(dst, r.toUtf8().constData(), cap - 1);
            dst[cap - 1] = '\0';
        };
        auto &w = m_engine->world();
        w.each([&](flecs::entity, ScriptComponent &sc)
               { fix(sc.filePath, sizeof(sc.filePath)); });
        w.each([&](flecs::entity, AnimationComponent &ac)
               { fix(ac.animFile, sizeof(ac.animFile)); });
        w.each([&](flecs::entity, SpriteComponent &sp)
               { fix(sp.folder, sizeof(sp.folder)); });
    }

    preloadSceneResources(m_engine->world());
    m_host->restartAutoplay();
    return true;
}

// ============================================================
// 输入接线（与编辑器同一套 Input 写入口）
// ============================================================
bool GameWindow::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::KeyPress:
    {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape)
        {
            close(); // 游戏里 Esc = 退出（最简策略，可由脚本演化成暂停菜单）
            return true;
        }
        m_host->routeKey(ke, true);
        return false;
    }
    case QEvent::KeyRelease:
        m_host->routeKey(static_cast<QKeyEvent *>(event), false);
        return false;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        processMouse(static_cast<QMouseEvent *>(event));
        return false;
    case QEvent::Wheel:
        m_host->routeWheel(static_cast<QWheelEvent *>(event));
        return false;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, event);
}

void GameWindow::processMouse(QMouseEvent *event)
{
    m_host->routeMouse(event);
}
