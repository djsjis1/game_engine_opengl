#include "GameWindow.h"
#include "Render/RenderWidget.h"
#include "Render/SceneSerializer.h"
#include "Render/AnimationLibrary.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/GamePaths.h"
#include "Core/SceneRouter.h" // 运行时切场景（change_scene 帧末执行）
#include "Systems/RotationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"

#include <QApplication>
#include <QCursor>
#include <QMessageBox>
#include <QDir>
#include <fstream>
#include <nlohmann/json.hpp>

GameWindow::GameWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Game");
    resize(1280, 720);

    // ====== 引擎 + 系统（与编辑器同一套 System，少了编辑器 UI）======
    m_engine = new Engine;
    m_engine->addSystem(std::make_unique<RotationSystem>());
    m_engine->addSystem(std::make_unique<CameraSystem>()); // 场景若带飞行相机可用
    m_engine->addSystem(std::make_unique<TransformSystem>());
    m_engine->addSystem(std::make_unique<AnimationSystem>());
    auto *physicsSys = new PhysicsSystem;
    physicsSys->init();
    m_engine->addSystem(std::unique_ptr<PhysicsSystem>(physicsSys));
    auto *scriptSys = new ScriptSystem;
    scriptSys->init(m_engine->world());
    m_engine->addSystem(std::unique_ptr<ScriptSystem>(scriptSys));

    // 进 Play 的回调：启停 System + autoplay 动画（无编辑器 UI 部分）
    m_engine->context().onEnterPlay = [this]()
    {
        m_engine->onEnterPlay();
        m_engine->world().each([&](flecs::entity, AnimationComponent &ac)
                               {
            if (ac.autoplay && ac.animFile[0] && ac.current[0])
            {
                AnimLib *lb = AnimationLibraryManager::instance().getOrCreate(ac.animFile);
                if (!lb || !lb->clips.count(ac.current)) return;
                ac.playing = true;
                ac.time = 0.0f;
            } });
    };

    // ====== 视口（唯一 UI：整个窗口就是游戏画面）======
    m_renderWidget = new RenderWidget(this);
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setShowGizmo(false); // 玩家不该看到编辑器网格
    m_renderWidget->setMouseTracking(true);
    setCentralWidget(m_renderWidget);
    setMouseTracking(true);
    qApp->installEventFilter(this);

    // ====== 加载游戏内容 → 直接进 Play（玩家没有"编辑模式"）======
    loadGame();
    m_engine->setPlay();

    // ====== 游戏循环（与编辑器同节奏，约 60FPS）======
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
        float rawDt = m_elapsed.restart() / 1000.f;
        auto &time = Time::instance();
        time.step(rawDt);
        auto &in = Input::instance();

        m_engine->runFrame(time.delta);

        // 光标捕获（FPS 锁中心）：游戏里始终尊重脚本的 set_mouse_mode
        bool capture = (in.mouseMode() == Input::kMouseCaptured);
        if (capture != m_cursorCaptured)
        {
            m_cursorCaptured = capture;
            m_renderWidget->setCursor(capture ? Qt::BlankCursor : Qt::ArrowCursor);
        }
        if (capture)
        {
            QPoint center = m_renderWidget->mapToGlobal(
                QPoint(m_renderWidget->width() / 2, m_renderWidget->height() / 2));
            QCursor::setPos(center);
            m_lastMousePos = center; // 防"跳回中心"假位移
        }

        in.endFrame();

        // ★ 帧末执行切场景（脚本 change_scene 的请求）：
        //   此刻所有 System 跑完、没人在遍历实体，销毁/重建安全
        if (SceneRouter::instance().hasPending())
            loadScene(QString::fromStdString(SceneRouter::instance().takePending()));

        m_renderWidget->update(); });
}

GameWindow::~GameWindow()
{
    // 先解绑回调再析构：FSM 析构会触发 exit，回调若引用已亡对象 → UAF
    // （教训来自编辑器 ~MainWindow，同一坑不踩第二次）
    m_engine->context() = EngineContext{};
    if (m_timer)
        m_timer->stop();
    delete m_engine;
}

void GameWindow::startGameLoop()
{
    m_elapsed.start();
    m_timer->start(16);
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

    // autoplay 动画重触发（切关后新场景的动画也要自动开播）
    m_engine->world().each([&](flecs::entity, AnimationComponent &ac)
                           {
        if (ac.autoplay && ac.animFile[0] && ac.current[0])
        {
            AnimLib *lb = AnimationLibraryManager::instance().getOrCreate(ac.animFile);
            if (!lb || !lb->clips.count(ac.current)) return;
            ac.playing = true;
            ac.time = 0.0f;
        } });
    return true;
}

// ============================================================
// 输入接线（与编辑器同一套 Input 写入口）
// ============================================================
bool GameWindow::eventFilter(QObject *obj, QEvent *event)
{
    auto &in = Input::instance();
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
        in.onKey(ke->key(), true);
        return false;
    }
    case QEvent::KeyRelease:
        in.onKey(static_cast<QKeyEvent *>(event)->key(), false);
        return false;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        processMouse(static_cast<QMouseEvent *>(event));
        return false;
    case QEvent::Wheel:
        in.onWheel((float)static_cast<QWheelEvent *>(event)->angleDelta().y() / 120.0f);
        return false;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, event);
}

void GameWindow::processMouse(QMouseEvent *event)
{
    auto &in = Input::instance();
    auto btnIndex = [](Qt::MouseButton b) -> int
    {
        if (b == Qt::LeftButton)
            return 0;
        if (b == Qt::MiddleButton)
            return 1;
        if (b == Qt::RightButton)
            return 2;
        return -1;
    };
    if (event->type() == QEvent::MouseButtonPress)
    {
        int idx = btnIndex(event->button());
        if (idx >= 0)
            in.onMouseButton(idx, true);
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        int idx = btnIndex(event->button());
        if (idx >= 0)
            in.onMouseButton(idx, false);
    }
    else if (event->type() == QEvent::MouseMove)
    {
        in.onMousePosition((float)event->x(), (float)event->y());
        if (!m_lastMousePos.isNull())
        {
            QPoint d = event->globalPos() - m_lastMousePos;
            in.onMouseMove((float)d.x(), (float)d.y());
        }
        m_lastMousePos = event->globalPos();
    }
}
