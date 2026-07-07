#ifndef ENGINE_SYSTEMS_CAMERA_SYSTEM_H
#define ENGINE_SYSTEMS_CAMERA_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/camera.h"
#include "../components/transform.h"
#include <QMatrix4x4>
#include <QVector3D>

// ============================================================
// CameraSystem: 摄像机管理
//
// 遍历 registry 找到活跃 CameraComponent，计算 view/projection。
// 不继承 System 基类，直接通过 update(registry) 调用。
// ============================================================
class CameraSystem
{
public:
    void update(entt::registry &registry, float /*dt*/)
    {
        float aspectRatio = m_aspectRatio;

        auto view = registry.view<CameraComponent>();
        for (auto entity : view)
        {
            auto &camera = view.get<CameraComponent>(entity);
            if (!camera.active)
                continue;

            // 位置优先从 TransformComponent 取
            QVector3D pos = camera.position;
            QVector3D front = camera.front;
            QVector3D up = camera.up;

            if (auto *t = registry.try_get<TransformComponent>(entity))
            {
                pos = t->position;

                if (t->rotation.x() != 0 || t->rotation.y() != 0 || t->rotation.z() != 0)
                {
                    QMatrix4x4 rotMat;
                    rotMat.rotate(t->rotation.x(), 1, 0, 0);
                    rotMat.rotate(t->rotation.y(), 0, 1, 0);
                    rotMat.rotate(t->rotation.z(), 0, 0, 1);
                    QVector3D defaultFront(0, 0, -1);
                    front = (rotMat * defaultFront).normalized();

                    QVector3D worldUp(0, 1, 0);
                    QVector3D right = QVector3D::crossProduct(front, worldUp).normalized();
                    up = QVector3D::crossProduct(right, front).normalized();
                }
            }

            m_viewMatrix.setToIdentity();
            m_viewMatrix.lookAt(pos, pos + front, up);

            m_projectionMatrix.setToIdentity();
            m_projectionMatrix.perspective(camera.fov, aspectRatio,
                                           camera.nearPlane, camera.farPlane);

            m_cameraPosition = pos;
            break; // 只处理第一个活跃摄像机
        }
    }

    const QMatrix4x4 &getViewMatrix() const { return m_viewMatrix; }
    const QMatrix4x4 &getProjectionMatrix() const { return m_projectionMatrix; }
    const QVector3D &getCameraPosition() const { return m_cameraPosition; }

    void setAspectRatio(float ratio) { m_aspectRatio = ratio; }

private:
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_projectionMatrix;
    QVector3D m_cameraPosition;
    float m_aspectRatio = 1.0f;
};

#endif // ENGINE_SYSTEMS_CAMERA_SYSTEM_H
