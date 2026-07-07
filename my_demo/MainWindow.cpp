// ============================================================
// MainWindow.cpp —— 主窗口（程序的"指挥官"）
//
// 【角色】把引擎、编辑栏、渲染器、输入全部组装到一起
//
// 【它做的事】
//   1. 创建 Engine + 注册所有 System
//   2. 组装 UI 布局（左边编辑栏，右边渲染视口）
//   3. QTimer(16ms) 驱动游戏循环
//   4. 全局事件过滤器捕获键盘/鼠标（不管焦点在哪个控件）
//   5. 每帧：注入输入 → runFrame → 清除增量 → 刷新渲染
//
// 【游戏循环（每 16ms）】
//   m_cameraSys->setInput(&m_input)     // 把最新键鼠状态注入摄像机系统
//   m_engine->runFrame(dt)              // 所有 System 按顺序跑一遍
//   m_input.resetFrame()                // 清除本帧鼠标增量（避免累积）
//   m_renderWidget->update()            // 触发 paintGL() 重绘
//
// 【扩展指南】
//   新增 System   → 构造函数里加 engine.addSystem(new XxxSystem)
//   新增按键      → processKey() 加 case
//   修改帧率      → m_timer->start(16) 改数值（8=125FPS, 32≈30FPS）
// ============================================================

#include "MainWindow.h"
#include "RenderWidget.h"
#include "Engine.h"
#include "editor/EditorPanel.h"
#include "System/CameraSystem.h"
#include "System/RotationSystem.h"
#include "System/MeshFactory.h"

#include <QHBoxLayout>
#include <QCursor>
#include <QEvent>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("My Demo Engine");
    resize(1200, 750);

    // ====== 1. 引擎初始化 ======
    // Engine 构造函数里注册了所有组件类型
    m_engine = new Engine;

    // System 注册顺序 = 执行顺序
    // 【重要】顺序影响结果：先旋转物体，再移动摄像机
    m_engine->addSystem(new RotationSystem); // 第 1 个执行

    m_cameraSys = new CameraSystem;   // 保存指针，后续每帧注入输入
    m_engine->addSystem(m_cameraSys); // 第 2 个执行

    // ====== 2. UI 布局 ======
    // 水平布局：[编辑栏(固定360px)] | [渲染视口(自适应)]
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    // -- 左侧编辑栏 --
    m_editor = new EditorPanel(central);
    m_editor->setWorld(&m_engine->world()); // 传入 ECS 世界引用

    // Mesh 创建回调：编辑栏添加 Mesh 组件时调用
    // 需要 makeCurrent 因为 OpenGL 操作必须在活跃上下文中
    m_editor->setMeshCreator([this](flecs::entity e)
                             {
                                 m_renderWidget->makeCurrent(); // 激活 GL 上下文
                                 MeshFactory factory;
                                 MeshComponent mesh;
                                 factory.createCube(mesh); // 生成立方体 VAO 数据
                                 e.set<MeshComponent>(mesh);
                                 m_renderWidget->doneCurrent(); // 释放 GL 上下文
                             });

    layout->addWidget(m_editor);

    // -- 右侧渲染视口 --
    m_renderWidget = new RenderWidget(central);
    m_renderWidget->setWorld(&m_engine->world());
    m_renderWidget->setMouseTracking(true); // 启用鼠标追踪（不需要点击就能收到移动事件）
    layout->addWidget(m_renderWidget, 1);   // stretch=1 → 占满剩余空间
    setCentralWidget(central);
    setMouseTracking(true);

    // ====== 3. 全局事件过滤器 ======
    // 为什么不用 keyPressEvent？
    // → 因为点击编辑栏的输入框后，焦点被 QDoubleSpinBox 抢走，
    //    MainWindow 的 keyPressEvent 不再收到事件
    // installEventFilter(this) 在 QApplication 级别捕获，绕过焦点问题
    // 返回 false：让事件继续传给子控件（编辑栏输入框还能正常打字）
    qApp->installEventFilter(this);

    // ====== 4. 编辑栏 → 渲染联动 ======
    // 任何编辑栏参数变化 → 立即刷新渲染
    connect(m_editor, &EditorPanel::valueChanged,
            m_renderWidget, QOverload<>::of(&QWidget::update));

    // ====== 5. 游戏循环（心跳） ======
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
                // 计算帧间隔（秒）
                float dt = m_elapsed.restart() / 1000.f;
                if (dt > 0.1f)
                    dt = 0.016f; // 防止首帧/暂停后帧间隔过大

                // ★ 每帧核心逻辑 ★
                m_cameraSys->setInput(&m_input); // 1. 注入最新键鼠状态
                m_engine->runFrame(dt);          // 2. 执行所有 System
                m_input.resetFrame();            // 3. 清除鼠标增量（避免累积到下一帧）
                m_renderWidget->update();        // 4. 触发 paintGL 重绘
            });

    // ====== 6. 自动创建默认摄像机 ======
    // 没有摄像机 = 看不到东西，所以启动时自动创建一个
    auto camera = m_engine->world().entity();
    camera.set<TagComponent>({"Camera"}); // 名称
    camera.set<TransformComponent>({});   // 位置（默认 0,0,0）
    auto camComp = CameraComponent{};
    camComp.isEditor = true;              // 编辑器摄像机，由 WASD/鼠标控制
    camera.set<CameraComponent>(camComp); // 摄像机参数
}

// ---- 启动游戏循环 ----
// 在 window.show() 之后调用，确保 OpenGL 上下文已创建
void MainWindow::startGameLoop()
{
    m_elapsed.start();
    m_timer->start(16); // 16ms ≈ 60 FPS
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this); // 卸载过滤器
    m_timer->stop();
    m_renderWidget->cleanupGL(); // 先清理 GPU 资源
    delete m_engine;             // 再删引擎（释放内存）
}

// ============================================================
// 全局事件过滤器 —— 捕获所有键盘/鼠标事件
// ============================================================
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::KeyPress:
        processKey(static_cast<QKeyEvent *>(event), true);
        return false; // ⚠ 返回 false，让子控件也能处理事件
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
    // 调用父类，确保 Qt 正常处理其他事件（关闭窗口等）
    return QMainWindow::eventFilter(obj, event);
}

// ---- 键盘 → CameraInput ----
void MainWindow::processKey(QKeyEvent *event, bool pressed)
{
    switch (event->key())
    {
    case Qt::Key_W:
        m_input.forward = pressed;
        break;
    case Qt::Key_S:
        m_input.backward = pressed;
        break;
    case Qt::Key_A:
        m_input.left = pressed;
        break;
    case Qt::Key_D:
        m_input.right = pressed;
        break;
    case Qt::Key_Q:
        m_input.up = pressed;
        break; // 上升
    case Qt::Key_E:
        m_input.down = pressed;
        break; // 下降
    }
}

// ---- 鼠标 → CameraInput ----
void MainWindow::processMouse(QMouseEvent *event)
{
    // 右键按下/松开 → 启用/禁用旋转
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::RightButton)
        m_input.rightMouseDown = true;
    else if (event->type() == QEvent::MouseButtonRelease && event->button() == Qt::RightButton)
        m_input.rightMouseDown = false;
    // 鼠标移动 → 累加增量（每帧 resetFrame 清零）
    else if (event->type() == QEvent::MouseMove)
    {
        static QPoint lastPos; // static：跨帧保持上次位置
        if (lastPos.isNull())
        {
            lastPos = event->globalPos();
            return;
        } // 首次跳过
        QPoint delta = event->globalPos() - lastPos;
        m_input.mouseDX += (float)delta.x();
        m_input.mouseDY += (float)delta.y();
        lastPos = event->globalPos();
    }
}
