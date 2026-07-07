#ifndef ENGINE_COMPONENTS_CAMERA_H
#define ENGINE_COMPONENTS_CAMERA_H

#include <QVector3D>
#include <QMatrix4x4>

// ============================================================
// CameraComponent: 摄像机数据
//
// 存储摄像机参数和观察矩阵。
// CameraSystem 使用这些数据生成 view/projection 矩阵。
//
// 用法：场景中可能有多个摄像机，用 m_active 标记当前激活的。
// ============================================================
struct CameraComponent
{
    // 摄像机位置与朝向
    QVector3D position = QVector3D(0.0f, 0.0f, 5.0f);
    QVector3D front = QVector3D(0.0f, 0.0f, -1.0f); // 看向 -Z
    QVector3D up = QVector3D(0.0f, 1.0f, 0.0f);     // 世界 Y 轴向上

    // 投影参数
    float fov = 45.0f;       // 垂直视野角度（度）
    float nearPlane = 0.1f;  // 近裁剪面
    float farPlane = 100.0f; // 远裁剪面

    // 是否为当前激活的摄像机（由 CameraSystem 管理）
    bool active = true;

    // ---- 矩阵计算 ----

    // 生成 View 矩阵
    QMatrix4x4 getViewMatrix() const
    {
        QMatrix4x4 view;
        view.lookAt(position, position + front, up);
        return view;
    }

    // 生成 Projection 矩阵（需传入宽高比）
    QMatrix4x4 getProjectionMatrix(float aspectRatio) const
    {
        QMatrix4x4 proj;
        proj.perspective(fov, aspectRatio, nearPlane, farPlane);
        return proj;
    }
};

#endif // ENGINE_COMPONENTS_CAMERA_H
