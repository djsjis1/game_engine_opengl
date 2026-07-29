#include "MainWindow.h"
#include "Render/RenderWidget.h"
#include "Core/Engine.h"
#include "Editor/EditorPanel.h"
#include "Systems/RotationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/AnimationSystem.h"
#include "Scripting/LuaApi.h" // clearDeferredSignals（进/出 Play 清陈旧信号）
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Render/ResourceManager.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/CommandStack.h"
#include "Render/SceneSerializer.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QShortcut>
#include <QEvent>
#include <QApplication>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QCoreApplication>
#include <QDir>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Mini Engine");
    resize(1440, 860);

    // ====== 引擎 + 系统 ======
    m_engine = new Engine;
    m_engine->addSystem(std::make_unique<RotationSystem>());
    m_engine->addSystem(std::make_unique<CameraSystem>());
    m_engine->addSystem(std::make_unique<TransformSystem>()); // 父子层级变换传播
    m_engine->addSystem(std::make_unique<AnimationSystem>()); // 属性动画采样（脚本后、Transform前）
    auto *physicsSysPtr = new PhysicsSystem;
    physicsSysPtr->init();
    m_engine->addSystem(std::unique_ptr<PhysicsSystem>(physicsSysPtr));
    auto *scriptSysPtr = new ScriptSystem;
    scriptSysPtr->init(m_engine->world());
    m_engine->addSystem(std::unique_ptr<ScriptSystem>(scriptSysPtr));

    // ====== 状态机回调（引擎 Context 注册 HFSM2 enter/exit） ======
    m_engine->context().onEnterPlay = [this]()
    {
        m_engine->onEnterPlay(); // ★ applyStateFilter：按 Play 状态启停 System（Physics 开、等）
        clearDeferredSignals();  // ★ 丢弃编辑模式堆积的陈旧信号（预览动画事件无消费者）
        m_editor->setVisible(false);
        m_renderWidget->setShowGizmo(false);
        // ★ 快照场景：Play 期间物理/脚本改的位置，停止后全部还原
        m_engine->snapshotTransforms();
        // ★ autoplay：进入运行模式，勾了"自动播放"的动画从头开播
        //   （对标 Godot AnimationPlayer.autoplay —— 点运行即见动画，无需配置）
        int autoplayCount = 0;
        int badClip = 0; // 文件里找不到指定动画的实体数（路径错/名字错）
        m_engine->world().each([&](flecs::entity, AnimationComponent &ac)
                               {
            if (ac.autoplay && ac.animFile[0] && ac.current[0])
            {
                AnimLib *lb = AnimationLibraryManager::instance().getOrCreate(ac.animFile);
                if (!lb || !lb->clips.count(ac.current)) { badClip++; return; }
                ac.playing = true;
                ac.time = 0.0f;
                autoplayCount++;
            } });
        // 诊断可见化：WIN32 GUI 无控制台，状态栏是用户能看见的通道
        QString msg = QString("▶ 运行中 | 自动播放 %1 个动画").arg(autoplayCount);
        if (badClip > 0)
            msg += QString(" | ⚠ %1 个实体的动画文件/动画名无效（路径或拼写错）").arg(badClip);
        else if (autoplayCount == 0)
            msg += " | ⚠ 没有可播动画（检查：自动播放勾选/动画名非空）";
        statusBar()->showMessage(msg, 0);
    };
    m_engine->context().onExitPlay = [this, physicsSysPtr]()
    {
        m_engine->onExitPlay(); // ★ applyStateFilter：回 Edit 状态（Physics 停）
        clearDeferredSignals(); // 未消费的运行时信号不带回编辑模式
        m_editor->setVisible(true);
        m_renderWidget->setShowGizmo(true);
        // 销毁物理刹体 + 还原场景（对标 Godot：运行不污染编辑场景）
        physicsSysPtr->resetBodies();
        m_engine->restoreTransforms();
        // 所有动画停下来（含脚本 anim_play 启动的）：
        // 回编辑模式场景保持静止，预览由用户手动勾（Godot 同行为）
        m_engine->world().each([](flecs::entity, AnimationComponent &ac)
                               {
            ac.playing = false;
            ac.time = 0.0f; });
        statusBar()->showMessage("编辑模式", 3000); // 清掉"运行中"残留提示
    };
    // System 启用/禁用由 Engine::onEnterPlay/onExitEdit 自动处理

    // ====== UI ======
    // QSplitter：编辑面板与视口之间可拖拽分隔条调节宽度
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *splitter = new QSplitter(Qt::Horizontal, central);

    m_editor = new EditorPanel(splitter);
    m_editor->setWorld(&m_engine->world());
    // 注：ResourceManager 延迟化后，编辑器的资源操作无需 GL 上下文包裹
    splitter->addWidget(m_editor);

    m_renderWidget = new RenderWidget(splitter);
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setMouseTracking(true);
    splitter->addWidget(m_renderWidget);

    // 射线拾取接线：视口点选 → 编辑器列表同步（两者互不认识，只靠信号）
    connect(m_renderWidget, &RenderWidget::entityPicked, this, [this](quint64 id)
            {
        if (m_engine->state() == EngineState::Play) return; // 运行中不拾取
        m_editor->selectEntityById((flecs::entity_t)id); });

    splitter->setStretchFactor(0, 0); // 编辑面板：不自动拉伸
    splitter->setStretchFactor(1, 1); // 视口：吃掉窗口缩放的增量
    splitter->setSizes({420, 780});   // 初始宽度分配
    layout->addWidget(splitter);
    setCentralWidget(central);
    setMouseTracking(true);

    qApp->installEventFilter(this);

    // 工具栏
    auto *tb = addToolBar("Main");
    m_playAction = tb->addAction("▶ 运行");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::togglePlayMode);

    auto *saveAction = tb->addAction("💾 保存");
    connect(saveAction, &QAction::triggered, this, [this]()
            {
        QString path = QFileDialog::getSaveFileName(this, "保存场景", "scene.json", "JSON (*.json)");
        if (path.isEmpty()) return;
        // 保存是纯读数据写文件，无 GL 操作
        bool ok = SceneSerializer::save(m_engine->world(), path.toStdString());
        if (ok) statusBar()->showMessage("已保存: " + path, 3000); });

    auto *loadAction = tb->addAction("📂 加载");
    connect(loadAction, &QAction::triggered, this, [this]()
            {
        if (m_engine->state() == EngineState::Play) return; // 运行中不允许加载
        QString path = QFileDialog::getOpenFileName(this, "加载场景", "", "JSON (*.json)");
        if (path.isEmpty()) return;
        // 无需 GL 上下文：acquire/release 都是纯记账，GPU 操作延迟到渲染帧
        bool ok = SceneSerializer::load(m_engine->world(), path.toStdString());
        if (ok) {
            statusBar()->showMessage("已加载: " + path, 3000);
            m_editor->setWorld(&m_engine->world()); // 刷新组件下拉列表
            m_editor->refreshEntityList();          // 刷新实体列表（显示加载进来的实体）
            // 旧命令引用的实体已全部销毁，撤销历史作废
            CommandStack::instance().clear();
        } else {
            QMessageBox::warning(this, "加载失败", "无法解析场景文件");
        } });

    connect(m_editor, &EditorPanel::valueChanged,
            m_renderWidget, QOverload<>::of(&QWidget::update));

    // ====== 撤销/重做快捷键（仅编辑模式）======
    auto *undoSc = new QShortcut(QKeySequence::Undo, this); // Ctrl+Z
    connect(undoSc, &QShortcut::activated, this, [this]()
            {
        if (m_engine->state() == EngineState::Play) return;
        m_editor->undoLast(); });
    auto *redoSc = new QShortcut(QKeySequence::Redo, this); // Ctrl+Y / Ctrl+Shift+Z
    connect(redoSc, &QShortcut::activated, this, [this]()
            {
        if (m_engine->state() == EngineState::Play) return;
        m_editor->redoLast(); });

    // ====== 游戏循环（QTimer 驱动，约 60 FPS）======
    //
    // 每帧执行顺序：
    //   1. 算 dt（距上帧的真实秒数）
    //   2. Time::step → 更新全局时钟（脚本 delta() 读它）
    //   3. Engine::runFrame → 按 phase 顺序跑所有 System：
    //        Camera(50) → Physics(90) → Script/Rotation(100) → Transform(150)
    //   4. Input::endFrame → 清除本帧的“刚按下/刚松开”事件
    //   5. RenderWidget::update → 触发重绘（paintGL）
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
        float rawDt = m_elapsed.restart() / 1000.f; // 毫秒 → 秒

        auto &time = Time::instance();
        time.step(rawDt);

        auto &in = Input::instance();

        m_engine->runFrame(time.delta); // 驱动全部 System

        in.endFrame();                  // 清理一次性输入事件
        m_renderWidget->update(); }); // 请求重绘

    // ====== 编辑器摄像机实体 ======
    auto &w = m_engine->world();
    auto cam = w.entity();
    cam.set<TagComponent>({"EditorCamera"});
    cam.set<TransformComponent>({});
    auto cc = CameraComponent{};
    cc.isEditor = true;
    cam.set<CameraComponent>(cc);
}

void MainWindow::startGameLoop()
{
    m_elapsed.start();
    m_timer->start(16);

    // 创建动画测试实体（acquireMesh 是纯登记，GPU 创建延迟到首帧渲染）
    // 预挂 AnimationComponent 指向示例动画文件（含 spin/bounce 两个动画），
    // 选中实体即可在编辑器里直接测试：播放/擦洗/打帧/缓动/保存
    auto cube = m_engine->world().entity();
    cube.set<TagComponent>({"AnimCube"});
    cube.set<TransformComponent>({{0, 0, 2}, {0, 0, 0}, {1, 1, 1}});
    cube.set<MaterialComponent>({{0.2f, 0.2f, 0.8f}, {0.3f, 0.3f, 1}, {1, 1, 1}, 32});
    MeshComponent mesh;
    mesh.meshHandle = ResourceManager::instance().acquireMesh("cube");
    cube.set<MeshComponent>(mesh);
    AnimationComponent anim;
    // 绝对路径（ASSET_DIR 由 CMake 编译期注入）：不依赖工作目录，双击 exe 也能找到
    strncpy(anim.animFile, ASSET_DIR "anims/demo.anim", sizeof(anim.animFile) - 1);
    strncpy(anim.current, "spin", sizeof(anim.current) - 1);
    anim.playing = false; // 编辑模式静止；点▶运行时 autoplay 自动开播
    cube.set<AnimationComponent>(anim);
    // 精灵帧集：spin 动画含 SpriteComponent/frame 轨道 → 图片连续翻页
    // （一个动画同时驱动：旋转(vec3) + 颜色(vec3) + 帧号(int) 三种轨道）
    SpriteComponent sprite;
    strncpy(sprite.folder, ASSET_DIR "sprites/demo", sizeof(sprite.folder) - 1);
    cube.set<SpriteComponent>(sprite);

    // 启动时创建的实体完成后，立即刷新编辑器实体列表
    // （否则列表为空，直到用户首次点"新建实体"才一起冒出来，造成误导）
    m_editor->refreshEntityList();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    m_timer->stop();
    // ★ 先解绑状态机回调再销毁 Engine！（MODULES.md 坑7 的孪生陷阱）
    // Play 状态直接关窗口时：delete m_engine → FSM 析构 → 当前状态 exit
    // → onExitPlay 回调里的 physicsSysPtr/m_editor 可能已处于销毁流程
    // → use-after-free。清空回调后 FSM 析构静默退出，安全。
    m_engine->context() = EngineContext{};
    m_renderWidget->cleanupGL();
    delete m_engine;
}

void MainWindow::togglePlayMode()
{
    if (m_engine->state() == EngineState::Edit)
        m_engine->setPlay();
    else
        m_engine->setEdit();

    bool playing = m_engine->state() == EngineState::Play;
    m_playAction->setText(playing ? "■ 停止" : "▶ 运行");
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
        if (m_engine->state() == EngineState::Play &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
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
    Input::instance().onKey(event->key(), pressed);
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
