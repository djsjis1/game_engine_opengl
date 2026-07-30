#include "MainWindow.h"
#include "Render/RenderWidget.h"
#include "Core/Engine.h"
#include "Editor/EditorPanel.h"
#include "Editor/ScriptEditor.h"     // 内置脚本编辑器（主屏 Script 页）
#include "Scripting/ScriptApiStub.h" // 生成 VSCode API 存根
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
#include "Core/SceneRouter.h" // 运行时切场景（Play 模式帧末执行）
#include "Core/CommandStack.h"
#include "Render/SceneSerializer.h"

#include <QHBoxLayout>
#include <QSplitter>
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
        m_editor->inspectorWidget()->setVisible(false); // Inspector 也隐藏（独立 widget）
        m_renderWidget->setShowGizmo(false);
        m_renderWidget->setFocus(); // ★ 焦点交给视口：防工具栏控件吃游戏按键
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
        // ★ 快照场景：Play 期间物理/脚本改的位置，停止后全部还原
        m_engine->snapshotTransforms();
        // ★ 全量备份场景到临时文件：若 Play 中脚本 change_scene 换了关，
        //   退出时靠它恢复编辑中的场景（不备份 = 编辑工作直接丢！）
        SceneRouter::instance().resetChanged();
        m_sceneBeforePlay = SceneRouter::instance().current(); // 一并还原用
        SceneSerializer::save(m_engine->world(),
                              (QString(ASSET_DIR) + "scenes/__editor_backup.json").toStdString());
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
        m_editor->inspectorWidget()->setVisible(true);
        m_renderWidget->setShowGizmo(true);
        // 编辑器相机恢复（Play 时被让位给游戏相机）
        m_engine->world().each([&](flecs::entity, CameraComponent &cam)
                               { if (cam.isEditor) cam.active = true; });
        // 销毁物理刹体 + 还原场景（对标 Godot：运行不污染编辑场景）
        physicsSysPtr->resetBodies();
        if (SceneRouter::instance().changedInPlay())
        {
            // Play 中切过场景：当前世界已不是编辑时的 → 从备份整体恢复
            SceneSerializer::load(m_engine->world(),
                                  (QString(ASSET_DIR) + "scenes/__editor_backup.json").toStdString());
            SceneRouter::instance().resetChanged();
            SceneRouter::instance().setCurrent(m_sceneBeforePlay); // 场景名一并还原
            m_editor->refreshEntityList();
            CommandStack::instance().clear(); // 旧实体全没了，撤销历史作废
        }
        else
        {
            m_engine->restoreTransforms();
        }
        // 所有动画停下来（含脚本 anim_play 启动的）：
        // 回编辑模式场景保持静止，预览由用户手动勾（Godot 同行为）
        m_engine->world().each([](flecs::entity, AnimationComponent &ac)
                               {
            ac.playing = false;
            ac.time = 0.0f; });
        // 光标模式重置：脚本 set_mouse_mode("captured") 不能泄漏到编辑模式
        // （与上面停动画同类：临时状态退出时必须清理）
        Input::instance().setMouseMode(Input::kMouseVisible);
        statusBar()->showMessage("编辑模式", 3000); // 清掉"运行中"残留提示
    };
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

    // ---- 导出游戏（对标 Godot 导出）：给玩家真正的游戏，不是引擎框架 ----
    // 产物：目标目录/my_game.exe + game.json + assets/{scenes,scripts,...}
    // my_game.exe 启动即进 Play 直接跑主场景，无任何编辑器 UI
    auto *exportAction = tb->addAction("📦 导出游戏");
    connect(exportAction, &QAction::triggered, this, [this]()
            {
        // ① 选主场景（游戏启动时加载的那个）
        QString scene = QFileDialog::getOpenFileName(
            this, "选择主场景（游戏启动时加载）", ASSET_DIR "scenes", "JSON (*.json)");
        if (scene.isEmpty()) return;

        // ② 选导出目录
        QString outDir = QFileDialog::getExistingDirectory(this, "选择导出目录");
        if (outDir.isEmpty()) return;

        // ③ 拷运行时 exe（和编辑器同目录编出的 my_game.exe）
        QString gameExe = QCoreApplication::applicationDirPath() + "/my_game.exe";
        if (!QFile::exists(gameExe))
        {
            QMessageBox::warning(this, "导出失败",
                                 "找不到 my_game.exe（先构建 my_game 目标）");
            return;
        }
        QFile::remove(outDir + "/my_game.exe"); // 覆盖旧导出
        QFile::copy(gameExe, outDir + "/my_game.exe");

        // ④ 递归拷资源目录 → assets/（游戏资源跟着 exe 走）
        std::function<void(const QString &, const QString &)> copyDir =
            [&](const QString &src, const QString &dst)
        {
            QDir().mkpath(dst);
            QDir s(src);
            for (const auto &f : s.entryList(QDir::Files))
            {
                if (f.startsWith("__"))
                    continue; // 内部文件（Play 备份等）不进游戏包
                QFile::remove(dst + "/" + f);
                QFile::copy(src + "/" + f, dst + "/" + f);
            }
            for (const auto &d : s.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                copyDir(src + "/" + d, dst + "/" + d);
        };
        const char *assetDirs[] = {"scenes", "scripts", "anims",
                                   "sprites", "prefabs", "Shaders"};
        for (const char *d : assetDirs)
        {
            QString src = QString(ASSET_DIR) + d;
            if (QDir(src).exists())
                copyDir(src, outDir + "/assets/" + d);
        }

        // ⑤ 写游戏配置：主场景用相对路径（相对 assets/）
        QFile cfg(outDir + "/assets/game.json");
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QString name = QFileInfo(scene).fileName();
            cfg.write(QString("{ \"main_scene\": \"scenes/%1\" }\n")
                          .arg(name).toUtf8());
        }

        // ⑥ windeployqt 自动补齐 Qt DLL（找得到就跑，找不到提示手动拷）
        QString deployTool = QLibraryInfo::location(QLibraryInfo::BinariesPath)
                             + "/windeployqt.exe";
        QString dllHint;
        if (QFile::exists(deployTool))
        {
            QProcess::execute(deployTool, {outDir + "/my_game.exe"});
            dllHint = "Qt DLL 已自动部署。";
        }
        else
        {
            dllHint = "未找到 windeployqt，请手动拷贝 Qt DLL 到导出目录。";
        }

        QMessageBox::information(this, "导出完成",
            QString("游戏已导出到：%1\n主场景：%2\n%3\n\n"
                    "双击 my_game.exe 即可游玩（Esc 退出）")
                .arg(outDir, QFileInfo(scene).fileName(), dllHint)); });

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

        // 应用光标模式（脚本 set_mouse_mode 写 Input，UI 层这里读取执行）
        // 只在 Play 模式生效：编辑模式强制正常光标（不然没法操作编辑器）
        bool capture = (m_engine->state() == EngineState::Play &&
                        in.mouseMode() == Input::kMouseCaptured);
        if (capture != m_cursorCaptured)
        {
            m_cursorCaptured = capture;
            m_renderWidget->setCursor(capture ? Qt::BlankCursor : Qt::ArrowCursor);
        }
        if (capture)
        {
            // 每帧回中：光标锁在视口中心，相对位移照常累加（FPS 视角）
            QPoint center = m_renderWidget->mapToGlobal(
                QPoint(m_renderWidget->width() / 2, m_renderWidget->height() / 2));
            QCursor::setPos(center);
            // 重置参考点：setPos 会触发 MouseMove，不重置会把
            // "跳回中心"算成一次巨大假位移（视角猛甩）
            m_lastMousePos = center;
        }

        in.endFrame();                  // 清理一次性输入事件

        // ★ 帧末执行切场景（仅 Play 模式；脚本 change_scene 的请求）
        if (m_engine->state() == EngineState::Play &&
            SceneRouter::instance().hasPending())
        {
            QString rel = QString::fromStdString(SceneRouter::instance().takePending());
            if (SceneSerializer::load(m_engine->world(),
                                      (QString(ASSET_DIR) + rel).toStdString()))
            {
                SceneRouter::instance().setCurrent(rel.toStdString());
                SceneRouter::instance().markChanged(); // 退出 Play 时要恢复备份
                // 新场景有游戏相机则编辑器相机继续让位
                m_engine->world().each([&](flecs::entity, CameraComponent &cam)
                                       { if (cam.isEditor) cam.active = false; });
            }
        }
        else if (m_engine->state() != EngineState::Play)
        {
            SceneRouter::instance().takePending(); // 编辑模式丢弃残留请求
        }

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
    Input::instance().onKey(event->key(), pressed);
}

void MainWindow::processMouse(QMouseEvent *event)
{
    auto &in = Input::instance();
    // Qt 鼠标键 → 引擎下标（0=左 1=中 2=右）
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
        // 绝对坐标（供脚本 get_mouse_position）
        in.onMousePosition((float)event->x(), (float)event->y());
        // 相对位移（供相机转视角）；成员变量而非 static，
        // 光标回中后可重置，避免"跳回中心"被当成真位移
        if (!m_lastMousePos.isNull())
        {
            QPoint d = event->globalPos() - m_lastMousePos;
            in.onMouseMove((float)d.x(), (float)d.y());
        }
        m_lastMousePos = event->globalPos();
    }
}

void MainWindow::processWheel(QWheelEvent *event)
{
    // angleDelta().y() 单位 1/8 度，常见一格 120 → 归一化为 ±1
    Input::instance().onWheel((float)event->angleDelta().y() / 120.0f);
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
