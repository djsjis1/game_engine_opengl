#ifndef ENGINE_ENGINE_WIDGET_H
#define ENGINE_ENGINE_WIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QTimer>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QSet>
#include <QtMath>
#include <functional>
#include <algorithm>
#include <cmath>
#include "engine.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/light.h"

// ============================================================
// EngineWidget: Qt OpenGL 渲染宿主
//
// 职责：
//   1. 提供 QOpenGLWidget + OpenGL 上下文
//   2. 驱动 Engine 的帧循环
//   3. 处理摄像机输入（WASD + 鼠标 + 滚轮）
//
// 摄像机控制：
//   用户通过 setCameraEntity() 指定哪个实体是摄像机，
//   Widget 自动处理该实体的 TransformComponent 和 CameraComponent。
//
// 使用流程：
//   1. 创建 EngineWidget
//   2. widget.engine().init(shaderPath)
//   3. 用 widget.engine() 创建场景（实体、组件）
//   4. widget.setCameraEntity(camEntity)
//   5. widget.show()
// ============================================================
class EngineWidget : public QOpenGLWidget,
                     protected QOpenGLFunctions_3_3_Core
{
public:
    explicit EngineWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]()
                { update(); });
    }

    ~EngineWidget()
    {
        makeCurrent();
        m_engine.cleanupGL();
        doneCurrent();
    }

    // 获取引擎引用（用户在此构建场景）
    Engine &engine() { return m_engine; }

    // 设置当前控制的摄像机实体
    void setCameraEntity(entt::entity entity) { m_cameraEntity = entity; }

    // 设置引擎就绪回调（initializeGL 中 engine.init() 之后调用）
    using InitCallback = std::function<void(Engine &)>;
    void setInitCallback(InitCallback cb) { m_initCallback = std::move(cb); }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);

        m_engine.init(SHADER_DIR);
        if (m_initCallback)
            m_initCallback(m_engine);
        m_elapsedTimer.start();
        m_timer->start(16);
    }

    void resizeGL(int w, int h) override
    {
        glViewport(0, 0, w, h);
        m_engine.setAspectRatio(static_cast<float>(w) / std::max(h, 1));
    }

    void paintGL() override
    {
        float dt = m_elapsedTimer.elapsed() / 1000.0f - m_lastFrameTime;
        m_lastFrameTime += dt;

        processKeyboard(dt);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_engine.update(dt);
        m_engine.render();
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
        {
            m_mousePressed = true;
            m_lastMousePos = e->pos();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            m_mousePressed = false;
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_mousePressed)
            return;

        QPoint delta = e->pos() - m_lastMousePos;
        m_lastMousePos = e->pos();

        m_yaw += delta.x() * 0.2f;
        m_pitch -= delta.y() * 0.2f;
        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;

        updateCameraFront();
    }

    void wheelEvent(QWheelEvent *e) override
    {
        m_fov -= e->angleDelta().y() / 120.0f;
        if (m_fov < 5.0f)
            m_fov = 5.0f;
        if (m_fov > 90.0f)
            m_fov = 90.0f;

        if (m_cameraEntity != entt::null)
            m_engine.getComponent<CameraComponent>(m_cameraEntity).fov = m_fov;
    }

    void keyPressEvent(QKeyEvent *e) override { m_keysPressed.insert(e->key()); }
    void keyReleaseEvent(QKeyEvent *e) override { m_keysPressed.remove(e->key()); }

private:
    void updateCameraFront()
    {
        if (m_cameraEntity == entt::null)
            return;

        QVector3D front;
        float yr = qDegreesToRadians(m_yaw);
        float pr = qDegreesToRadians(m_pitch);
        front.setX(cos(yr) * cos(pr));
        front.setY(sin(pr));
        front.setZ(sin(yr) * cos(pr));
        front.normalize();

        m_engine.getComponent<CameraComponent>(m_cameraEntity).front = front;
    }

    void processKeyboard(float dt)
    {
        if (m_cameraEntity == entt::null)
            return;

        auto &t = m_engine.getComponent<TransformComponent>(m_cameraEntity);
        auto &cam = m_engine.getComponent<CameraComponent>(m_cameraEntity);
        float speed = m_moveSpeed * dt;

        QVector3D right = QVector3D::crossProduct(cam.front, QVector3D(0, 1, 0)).normalized();

        if (m_keysPressed.contains(Qt::Key_W))
            t.position += cam.front * speed;
        if (m_keysPressed.contains(Qt::Key_S))
            t.position -= cam.front * speed;
        if (m_keysPressed.contains(Qt::Key_A))
            t.position -= right * speed;
        if (m_keysPressed.contains(Qt::Key_D))
            t.position += right * speed;
        if (m_keysPressed.contains(Qt::Key_Q))
            t.position -= QVector3D(0, 1, 0) * speed;
        if (m_keysPressed.contains(Qt::Key_E))
            t.position += QVector3D(0, 1, 0) * speed;

        cam.position = t.position;
    }

    Engine m_engine;
    InitCallback m_initCallback;

    entt::entity m_cameraEntity{entt::null};

    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsedTimer;
    float m_lastFrameTime = 0;

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_fov = 45.0f;
    float m_moveSpeed = 5.0f;

    bool m_mousePressed = false;
    QPoint m_lastMousePos;
    QSet<int> m_keysPressed;
};

#endif // ENGINE_ENGINE_WIDGET_H
