#ifndef GAME_ENGINE_EDITOR_VIEWPORT_H
#define GAME_ENGINE_EDITOR_VIEWPORT_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QTimer>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QSet>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../engine/engine.h"

// ============================================================
// Viewport: 3D 视口 (QOpenGLWidget)
//
// WASD 移动 + 右键鼠标旋转 + 滚轮缩放摄像机。
// 选中实体显示线框高亮 (TODO)。
// ============================================================
class Viewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit Viewport(Engine *engine, QWidget *parent = nullptr)
        : QOpenGLWidget(parent), m_engine(engine)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    void setCameraEntity(ecs_entity_t e) { m_cameraEntity = e; }
    ecs_entity_t selectedEntity() const { return m_selectedEntity; }
    float fps() const { return m_fps; }

signals:
    void glReady(); // 发射于 initializeGL，通知 MainWindow 引擎可就绪

public slots:
    void onEntitySelected(ecs_entity_t e) { m_selectedEntity = e; }
    void makeCurrent() { QOpenGLWidget::makeCurrent(); }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);
        m_elapsed.start();
        emit glReady();
    }

    void resizeGL(int w, int h) override
    {
        glViewport(0, 0, w, h);
        m_engine->renderer().setAspectRatio(static_cast<float>(w) / std::max(h, 1));
    }

    void paintGL() override
    {
        float now = m_elapsed.elapsed() / 1000.0f;
        float dt = now - m_lastTime;
        m_lastTime = now;
        m_fps = 1.0f / std::max(dt, 0.001f);

        processKeyboard(dt);

        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        auto *world = m_engine->world().raw();
        m_engine->renderer().beginFrame(world);
        m_engine->renderer().renderEntities(world);

        // 选中实体线框高亮 (TODO: 完善实现)
        if (m_selectedEntity != 0)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::RightButton)
        {
            m_mousePressed = true;
            m_lastMousePos = e->pos();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::RightButton)
            m_mousePressed = false;
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_mousePressed)
            return;
        QPoint d = e->pos() - m_lastMousePos;
        m_lastMousePos = e->pos();
        m_yaw += d.x() * 0.2f;
        m_pitch -= d.y() * 0.2f;
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

        if (m_cameraEntity != 0)
        {
            auto *cam = static_cast<CameraComponent *>(
                ecs_get_mut(m_engine->world().raw(), m_cameraEntity, CameraComponent));
            if (cam)
                cam->fov = m_fov;
        }
    }

    void keyPressEvent(QKeyEvent *e) override { m_keys.insert(e->key()); }
    void keyReleaseEvent(QKeyEvent *e) override { m_keys.remove(e->key()); }

private:
    void updateCameraFront()
    {
        if (m_cameraEntity == 0)
            return;
        auto *cam = static_cast<CameraComponent *>(
            ecs_get_mut(m_engine->world().raw(), m_cameraEntity, CameraComponent));
        if (!cam)
            return;

        float yr = glm::radians(m_yaw);
        float pr = glm::radians(m_pitch);
        cam->front = glm::normalize(glm::vec3(
            cos(yr) * cos(pr),
            sin(pr),
            sin(yr) * cos(pr)));
    }

    void processKeyboard(float dt)
    {
        if (m_cameraEntity == 0)
            return;
        auto *t = static_cast<TransformComponent *>(
            ecs_get_mut(m_engine->world().raw(), m_cameraEntity, TransformComponent));
        auto *cam = static_cast<CameraComponent *>(
            ecs_get_mut(m_engine->world().raw(), m_cameraEntity, CameraComponent));
        if (!t || !cam)
            return;

        float speed = 5.0f * dt;
        glm::vec3 right = glm::normalize(glm::cross(cam->front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (m_keys.contains(Qt::Key_W))
            t->position += cam->front * speed;
        if (m_keys.contains(Qt::Key_S))
            t->position -= cam->front * speed;
        if (m_keys.contains(Qt::Key_A))
            t->position -= right * speed;
        if (m_keys.contains(Qt::Key_D))
            t->position += right * speed;
        if (m_keys.contains(Qt::Key_Q))
            t->position -= glm::vec3(0.0f, 1.0f, 0.0f) * speed;
        if (m_keys.contains(Qt::Key_E))
            t->position += glm::vec3(0.0f, 1.0f, 0.0f) * speed;

        cam->position = t->position;
    }

    Engine *m_engine;
    ecs_entity_t m_cameraEntity = 0;
    ecs_entity_t m_selectedEntity = 0;

    QElapsedTimer m_elapsed;
    float m_lastTime = 0.0f;
    float m_fps = 0.0f;
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_fov = 45.0f;

    bool m_mousePressed = false;
    QPoint m_lastMousePos;
    QSet<int> m_keys;
};

#endif // GAME_ENGINE_EDITOR_VIEWPORT_H
