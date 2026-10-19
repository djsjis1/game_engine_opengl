#include "MainWindow.h"
#include "Render/RenderWidget.h"
#include "Core/Engine.h"
#include "Core/EngineHost.h"
#include "editor/EditorPanel.h"
#include "editor/ScriptEditor.h"     // 内置脚本编辑器（主屏 Script 页）
#include "editor/GameExporter.h"     // 导出游戏（纯逻辑类，UI 只管对话框）
#include "Core/EngineLog.h"          // 输出面板：注册脚本错误 sink
#include "Scripting/ScriptApiStub.h" // 生成 VSCode API 存根
#include "Systems/TweenSystem.h"    // 补间动画（退 Play 清残留）
#include "Systems/AnimFsmSystem.h"  // 动画状态机（退 Play 清残留）
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/AudioSystem.h" // 音频系统（miniaudio）
#include "Render/ResourceManager.h"
#include "Core/Input.h"
#include "Core/SceneRouter.h" // 运行时切场景（Play 模式帧末执行）
#include "Core/CommandStack.h"
#include "editor/EditorCommands.h" // MoveEntityCommand（Gizmo 拖动撤销）
#include "Render/SceneSerializer.h"
#include "Render/SceneResourcePreloader.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QDockWidget>    // 输出面板（底部 dock）
#include <QPlainTextEdit> // 输出面板文本区
#include <QStackedWidget>
#include <QShortcut>
#include <QEvent>
#include <QApplication>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QLibraryInfo>
#include <functional> // 导出游戏的递归拷目录 lambda

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Mini Engine");
    resize(1440, 860);

    // ====== 引擎 + 系统 ======
    m_host = std::make_unique<EngineHost>(this, ScriptSystem::HotReloadMode::EditorOnly);
    m_engine = &m_host->engine();
    auto *physicsSysPtr = m_host->systems().physics;
    auto *scriptSysPtr = m_host->systems().script;
    auto *audioSysPtr = m_host->systems().audio;

    // ====== 状态机回调（引擎 Context 注册 HFSM2 enter/exit） ======
    m_host->setEnterPlayCallback([this]()
    {
        m_editor->setVisible(false);
        m_editor->inspectorWidget()->setVisible(false); // Inspector 也隐藏（独立 widget）
        m_renderWidget->setShowGizmo(false);
        m_renderWidget->setPlayMode(true); // 相机选择策略：用游戏相机
        m_renderWidget->setFocus();        // ★ 焦点交给视口：防工具栏控件吃游戏按键
        // ★ 编辑器相机让位（对标 Godot：运行看到的是游戏相机视角）：
        //   active=false → 不再吃 WASD（CameraSystem 只驱动激活的编辑器相机）
        //   → RenderWidget 自动落到场景里的游戏相机（若有）
        //   没有游戏相机的场景：仍用编辑器相机画面（但不吃输入）
        bool hasGameCam = false;
        m_engine->world().each([&](flecs::entity, CameraComponent &cam)
                               { if (!cam.isEditor && cam.active) hasGameCam = true; });
        if (hasGameCam)
            m_engine->world().each([&](flecs::entity, CameraComponent &cam)
                                   { if (cam.isEditor) cam.active = false; });
        // ★ 完整场景快照（对标 Godot：Play 期间随便造，Stop 后完美还原）
        m_engine->snapshotScene();
        SceneRouter::instance().resetChanged();
        m_sceneBeforePlay = SceneRouter::instance().current();
        // ★ autoplay：进入运行模式，勾了"自动播放"的动画从头开播
        //   （对标 Godot AnimationPlayer.autoplay —— 点运行即见动画，无需配置）
        const auto autoplay = m_host->restartAutoplay();
        // 诊断可见化：WIN32 GUI 无控制台，状态栏是用户能看见的通道
        QString msg = QString("▶ 运行中 | 自动播放 %1 个动画").arg(autoplay.started);
        if (autoplay.invalid > 0)
            msg += QString(" | ⚠ %1 个实体的动画文件/动画名无效（路径或拼写错）").arg(autoplay.invalid);
        else if (autoplay.started == 0)
            msg += " | ⚠ 没有可播动画（检查：自动播放勾选/动画名非空）";
        statusBar()->showMessage(msg, 0);
    });
    m_host->setExitPlayCallback([this, physicsSysPtr, audioSysPtr, scriptSysPtr]()
    {
        m_editor->setVisible(true);
        m_editor->inspectorWidget()->setVisible(true);
        m_renderWidget->setShowGizmo(true);
        m_renderWidget->setPlayMode(false); // 相机选择策略：回编辑器相机
        // 编辑器相机恢复（Play 时被让位给游戏相机）
        m_engine->world().each([&](flecs::entity, CameraComponent &cam)
                               { if (cam.isEditor) cam.active = true; });
        // 销毁物理刚体 + 完整还原场景（对标 Godot：运行不污染编辑场景）
        physicsSysPtr->resetBodies();
        m_engine->restoreScene(); // 全组件还原 + 运行时新建实体清除
        preloadSceneResources(m_engine->world());
        SceneRouter::instance().resetChanged();
        SceneRouter::instance().setCurrent(m_sceneBeforePlay);
        m_editor->refreshEntityList();
        CommandStack::instance().clear(); // 实体全重建，撤销历史作废
        // 所有动画停下来（含脚本 anim_play 启动的）：
        // 回编辑模式场景保持静止，预览由用户手动勾（Godot 同行为）
        m_engine->world().each([](flecs::entity, AnimationComponent &ac)
                               {
            ac.playing = false;
            ac.time = 0.0f; });
        // 光标模式重置：脚本 set_mouse_mode("captured") 不能泄漏到编辑模式
        // （与上面停动画同类：临时状态退出时必须清理）
        Input::instance().setMouseMode(Input::kMouseVisible);
        audioSysPtr->stopAll(); // 停所有音效/音乐（不漏到编辑模式，同停动画）
        // ★ 重置全部脚本环境：下次 Play 重跑 ready()、local 变量归零。
        //   不重置的话第二次 Play 视角不正常（ready 里的 set_mouse_mode/
        //   set_visible/yaw 初始化全部缺失，实bug）
        scriptSysPtr->resetAllScripts();
        TweenSystem::clearAll();                    // 未完成的补间清掉（不然下次 Play 物体乱跳）
        AnimFsmSystem::clearAll();                  // 状态机规则清掉（下次 Play ready() 重新声明）
        statusBar()->showMessage("编辑模式", 3000); // 清掉"运行中"残留提示
    });
    // System 启用/禁用由 Engine::onEnterPlay/onExitEdit 自动处理

    // ====== UI ======
    // Godot 风格三栏：场景树(左) | 3D视口(中) | Inspector(右)
    // QSplitter：三栏之间都可拖拽分隔条调节宽度
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *splitter = new QSplitter(Qt::Horizontal, central);

    m_editor = new EditorPanel(splitter); // 左：场景树
    m_editor->setWorld(&m_engine->world());
    // 注：ResourceManager 延迟化后，编辑器的资源操作无需 GL 上下文包裹
    splitter->addWidget(m_editor);

    m_renderWidget = new RenderWidget(splitter); // 中：3D 视口
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setMouseTracking(true);
    // ★ QOpenGLWidget 默认 NoFocus——不设 StrongFocus 的话
    //   进 Play 时 setFocus() 无效，焦点残留在工具栏按钮上
    m_renderWidget->setFocusPolicy(Qt::StrongFocus);

    // 中央区用 QStackedWidget 叠放两页：视口(0) / 脚本编辑器(1)
    // 对标 Godot 顶部 [3D][Script] 主屏切换
    m_centerStack = new QStackedWidget(splitter);
    m_scriptEditor = new ScriptEditor;
    connect(m_scriptEditor, &ScriptEditor::statusMessage, this,
            [this](const QString &msg)
            { statusBar()->showMessage(msg, 4000); });
    m_centerStack->addWidget(m_renderWidget); // index 0
    m_centerStack->addWidget(m_scriptEditor); // index 1
    splitter->addWidget(m_centerStack);

    splitter->addWidget(m_editor->inspectorWidget()); // 右：Inspector（全组件展开）

    // 射线拾取接线：视口点选 → 编辑器列表同步（两者互不认识，只靠信号）
    connect(m_renderWidget, &RenderWidget::entityPicked, this, [this](quint64 id)
            {
        if (m_engine->state() == EngineState::Play) return; // 运行中不拾取
        // 点到空白（id=0）不取消选择：误点不丢失编辑状态，
        // 不然每次手抖都得重新点实体才能继续改属性
        if (id == 0) return;
        m_editor->selectEntityById((flecs::entity_t)id); });

    // 双击资源面板的 .lua → 内置编辑器打开并跳 Script 页（解耦：信号）
    connect(m_editor, &EditorPanel::openScriptRequested, this,
            &MainWindow::openScriptInEditor);

    // 选中实体变化 → 视口相机预览小窗（选中带 CameraComponent 的实体时，
    // 右下角显示"它拍到的画面"——对标 Godot 选中 Camera3D 的 preview）
    // + 变换 Gizmo（选中实体显示移动箭头，视口直接拖）
    connect(m_editor, &EditorPanel::selectionChanged, this, [this](quint64 id)
            {
        m_renderWidget->setPreviewCamera((flecs::entity_t)id);
        m_renderWidget->setGizmoTarget((flecs::entity_t)id); });

    // Gizmo 拖完 → 包成撤销命令（Ctrl+Z 可回退）+ Inspector 数值同步刷新
    connect(m_renderWidget, &RenderWidget::gizmoTransformed, this,
            [this](quint64 id, int mode, float ox, float oy, float oz,
                   float nx, float ny, float nz)
            {
        CommandStack::instance().push(std::make_unique<TransformEditCommand>(
            &m_engine->world(), (flecs::entity_t)id, mode,
            glm::vec3(ox, oy, oz), glm::vec3(nx, ny, nz)));
        m_editor->refreshPropertyEditor(); });

    // 视口 Gizmo 模式切换（W/E/R）——不走 QShortcut！
    // ★ 实踩坑：QShortcut 优先级高于普通键盘事件，会先把 W 吞掉，
    //   事件到不了 Input → 编辑模式按 W 相机不动（“按鼠标/Ctrl 才能飞”
    //   的诡异现象就是：鼠标 grab 时快捷键被禁用 / Ctrl+W 不匹配单键）。
    //   改为在游戏循环里轮询 Input::isJustPressed —— 按键先进 Input，
    //   相机飞行与 Gizmo 切换同时生效（Godot 同行为）。

    // 热重载：内置编辑器 Ctrl+S 保存 → ScriptSystem 重载该脚本（运行中立即生效）
    connect(m_scriptEditor, &ScriptEditor::scriptSaved, this,
            [scriptSysPtr](const QString &path)
            { scriptSysPtr->reloadScript(path.toStdString()); });

    // 生成 VSCode 用的 API 存根（engine_api.lua）：写入脚本目录，
    // 用 VSCode 打开 scripts/ 写脚本即获得补全/参数提示/拼错报错
    ScriptApiStub::writeTo(ASSET_DIR "scripts");

    splitter->setStretchFactor(0, 0);    // 场景树：不自动拉伸
    splitter->setStretchFactor(1, 1);    // 视口：吃掉窗口缩放的增量
    splitter->setStretchFactor(2, 0);    // Inspector：不自动拉伸
    splitter->setSizes({260, 780, 400}); // 初始宽度：左窄 中大 右中
    layout->addWidget(splitter);
    setCentralWidget(central);
    setMouseTracking(true);

    // ====== 输出面板（对标 Godot Output）：脚本错误实时可见 ======
    // 引擎层只调 engineLogError()（零 Qt）；这里注册 UI sink 接进面板。
    // QueuedConnection 语义：invokeMethod 保证跨线程日志也安全落到 UI 线程
    auto *outputDock = new QDockWidget("输出", this);
    outputDock->setObjectName("OutputDock");
    m_outputPanel = new QPlainTextEdit(outputDock);
    m_outputPanel->setReadOnly(true);
    m_outputPanel->setMaximumBlockCount(500); // 只留最近 500 条，防无限增长
    outputDock->setWidget(m_outputPanel);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock);
    // 输出面板默认可见（方便看脚本 print 和错误信息）
    setEngineLogSink([this, outputDock](const std::string &msg)
                     {
        QString text = QString::fromStdString(msg);
        QMetaObject::invokeMethod(this, [this, outputDock, text]()
                                  {
            m_outputPanel->appendPlainText(text);
            if (!outputDock->isVisible())
                outputDock->show(); // 出错自动弹出（Godot 同行为）
                                  }, Qt::QueuedConnection); });

    qApp->installEventFilter(this);

    // 工具栏
    auto *tb = addToolBar("Main");

    // 主屏切换（对标 Godot 顶部 [3D][Script]）：checkable 互斥
    m_viewAction = tb->addAction("🎮 3D视口");
    m_viewAction->setCheckable(true);
    m_viewAction->setChecked(true);
    connect(m_viewAction, &QAction::triggered, this, &MainWindow::showViewport);
    m_scriptAction = tb->addAction("📜 脚本");
    m_scriptAction->setCheckable(true);
    connect(m_scriptAction, &QAction::triggered, this, &MainWindow::showScript);

    // 脚本编码选择（乱码时切编码；默认 UTF-8，切换后立即重读）
    auto *encLabel = new QLabel(" 编码:");
    tb->addWidget(encLabel);
    auto *encBox = new QComboBox;
    encBox->addItems({"UTF-8", "GBK", "System"}); // System=系统默认(Windows下即 GBK)
    encBox->setFocusPolicy(Qt::NoFocus);          // ★ 不抢键盘焦点：否则空格会弹开下拉框
    tb->addWidget(encBox);
    connect(encBox, &QComboBox::currentTextChanged, this,
            [this](const QString &enc)
            { if (m_scriptEditor) m_scriptEditor->setEncoding(enc); });
    tb->addSeparator();

    m_playAction = tb->addAction("▶ 运行");
    connect(m_playAction, &QAction::triggered, this, &MainWindow::togglePlayMode);

    auto *saveAction = tb->addAction("💾 保存场景");
    connect(saveAction, &QAction::triggered, this, [this]()
            {
        // 默认落 scenes/ 目录（绝对路径，对标 Godot 场景集中管理）
        QString path = QFileDialog::getSaveFileName(
            this, "保存场景", ASSET_DIR "scenes/scene.json", "JSON (*.json)");
        if (path.isEmpty()) return;
        bool ok = SceneSerializer::save(m_engine->world(), path.toStdString());
        if (ok) {
            statusBar()->showMessage("已保存: " + path, 3000);
            m_editor->refreshResourcePanel(); // ★ 新场景立即出现在资源面板
        } });

    auto *loadAction = tb->addAction("📂 加载场景");
    connect(loadAction, &QAction::triggered, this, [this]()
            {
        if (m_engine->state() == EngineState::Play) return; // 运行中不允许加载
        QString path = QFileDialog::getOpenFileName(
            this, "加载场景", ASSET_DIR "scenes", "JSON (*.json)");
        if (path.isEmpty()) return;
        // 无需 GL 上下文：acquire/release 都是纯记账，GPU 操作延迟到渲染帧
        bool ok = SceneSerializer::load(m_engine->world(), path.toStdString());
        if (ok) preloadSceneResources(m_engine->world());
        if (ok) {
            statusBar()->showMessage("已加载: " + path, 3000);
            // ★ 记录当前场景（相对资源根）：脚本 get_scene() 靠它判断在哪关。
            //   不设的话首次 change_scene 判断用空串 → 传送门第一次切错（实bug）
            SceneRouter::instance().setCurrent(
                QDir(ASSET_DIR).relativeFilePath(path).toStdString());
            m_editor->setWorld(&m_engine->world()); // 刷新组件下拉列表
            m_editor->refreshEntityList();          // 刷新场景树（显示加载进来的实体）
            // 旧命令引用的实体已全部销毁，撤销历史作废
            CommandStack::instance().clear();
        } else {
            QMessageBox::warning(this, "加载失败", "无法解析场景文件");
        } });

    connect(m_editor, &EditorPanel::valueChanged,
            m_renderWidget, QOverload<>::of(&QWidget::update));

    // ---- 导出游戏（对标 Godot 导出）----
    // UI 层只管三件事：选场景 → 选目录 → 展示结果。
    // 导出逻辑全在 editor/GameExporter.h（纯逻辑类，换 UI 框架不受影响）
    auto *exportAction = tb->addAction("📦 导出游戏");
    connect(exportAction, &QAction::triggered, this, [this]()
            {
        QString scene = QFileDialog::getOpenFileName(
            this, "选择主场景（游戏启动时加载）", ASSET_DIR "scenes", "JSON (*.json)");
        if (scene.isEmpty()) return;
        QString outDir = QFileDialog::getExistingDirectory(this, "选择导出目录");
        if (outDir.isEmpty()) return;

        auto result = GameExporter::exportGame(scene, outDir);
        if (!result.ok)
        {
            QMessageBox::warning(this, "导出失败", result.message);
            return;
        }
        QMessageBox::information(this, "导出完成",
            QString("游戏已导出到：%1\n主场景：%2\n%3\n\n"
                    "双击 my_game.exe 即可游玩（Esc 退出）")
                .arg(outDir, QFileInfo(scene).fileName(), result.message)); });

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

    m_host->setFpsCallback([this](float fps)
    {
        if (m_engine->state() != EngineState::Play) return;
        statusBar()->showMessage(
            QString("FPS: %1  |  帧时间: %2 ms")
                .arg(fps, 0, 'f', 1)
                .arg(1000.f / fps, 0, 'f', 2), 0);
    });
    m_host->setFrameCallback([this]()
    {
        auto &input = Input::instance();
        if (m_engine->state() == EngineState::Edit)
        {
            if (input.isJustPressed('W'))
            {
                m_renderWidget->setGizmoMode(0);
                statusBar()->showMessage("Gizmo：移动模式", 2000);
            }
            else if (input.isJustPressed('E'))
            {
                m_renderWidget->setGizmoMode(1);
                statusBar()->showMessage("Gizmo：旋转模式", 2000);
            }
            else if (input.isJustPressed('R'))
            {
                m_renderWidget->setGizmoMode(2);
                statusBar()->showMessage("Gizmo：缩放模式", 2000);
            }
        }
        m_host->applyMouseCapture(m_renderWidget, m_engine->state() == EngineState::Play);
        m_renderWidget->update();
    });
    m_host->setSceneChangeCallback([this](const std::string &path)
    {
        const QString rel = QString::fromStdString(path);
        const QString fullPath = (rel.size() >= 2 && rel[1] == ':') || rel.startsWith('/')
                                     ? rel : QString(ASSET_DIR) + rel;
        if (SceneSerializer::load(m_engine->world(), fullPath.toStdString()))
        {
            preloadSceneResources(m_engine->world());
            SceneRouter::instance().setCurrent(path);
            SceneRouter::instance().markChanged();
            m_engine->world().each([](flecs::entity, CameraComponent &camera)
                                   { if (camera.isEditor) camera.active = false; });
            m_host->restartAutoplay();
        }
    });

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
    m_host->start();

    // 创建动画测试实体（acquireMesh 是纯登记，GPU 创建延迟到首帧渲染）
    // 预挂 AnimationComponent 指向示例动画文件（含 spin/bounce 两个动画），
    // 选中实体即可在编辑器里直接测试：播放/擦洗/打帧/缓动/保存
    auto cube = m_engine->world().entity();
    cube.set<TagComponent>({"AnimCube"});
    cube.set<TransformComponent>({{0, 0, 2}, {0, 0, 0}, {1, 1, 1}});
    cube.set<MaterialComponent>({{0.3f, 0.3f, 1.0f}, 0.0f, 0.5f, 1.0f}); // PBR：蓝色非金属
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
    m_host->stop();
    if (m_renderWidget->context())
        m_renderWidget->cleanupGL();
    m_renderWidget->setWorld(nullptr);
    m_host.reset();
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
    switch (event->type())
    {
    case QEvent::KeyPress:
        processKey(static_cast<QKeyEvent *>(event), true);
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Tab)
            togglePlayMode();
        if (m_engine->state() == EngineState::Play &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
            togglePlayMode();
        // ★ Play 模式拦截按键：Input 已记录，不再传播给 UI 控件——
        //   否则空格弹开工具栏下拉框、W 触发控件首字母匹配（实bug）
        if (m_engine->state() == EngineState::Play)
            return true;
        return false;
    case QEvent::KeyRelease:
        processKey(static_cast<QKeyEvent *>(event), false);
        if (m_engine->state() == EngineState::Play)
            return true; // 同上：Play 模式按键不进 UI
        return false;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        processMouse(static_cast<QMouseEvent *>(event));
        return false;
    case QEvent::Wheel:
        processWheel(static_cast<QWheelEvent *>(event));
        return false;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::processKey(QKeyEvent *event, bool pressed)
{
    m_host->routeKey(event, pressed);
}

void MainWindow::processMouse(QMouseEvent *event)
{
    m_host->routeMouse(event);
}

void MainWindow::processWheel(QWheelEvent *event)
{
    m_host->routeWheel(event);
}

// ============================================================
// 主屏切换（对标 Godot 顶部 [3D][Script]）
// ============================================================
void MainWindow::showViewport()
{
    m_centerStack->setCurrentIndex(0); // 视口页
    m_viewAction->setChecked(true);
    m_scriptAction->setChecked(false);
}

void MainWindow::showScript()
{
    m_centerStack->setCurrentIndex(1); // 脚本页
    m_viewAction->setChecked(false);
    m_scriptAction->setChecked(true);
}

// 资源面板双击 .lua → 打开到内置编辑器并切到 Script 页
void MainWindow::openScriptInEditor(const QString &path)
{
    m_scriptEditor->loadFile(path);
    showScript(); // 自动切到脚本页（不用外部程序，对标 Godot）
}
