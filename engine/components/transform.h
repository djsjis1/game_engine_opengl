#ifndef ENGINE_COMPONENTS_TRANSFORM_H
#define ENGINE_COMPONENTS_TRANSFORM_H

#include <QVector3D>
#include <QMatrix4x4>
#include <cmath>

// ============================================================
// TransformComponent: 空间变换数据
//
// 存储实体在世界空间中的位置、旋转（欧拉角，度）、缩放。
// getModelMatrix() 将三者组合为 Model 矩阵（列优先，适配 OpenGL）。
//
// 变换顺序：先缩放 → 再旋转 → 最后平移（经典 TRS 顺序）
// ============================================================
struct TransformComponent
{
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D rotation{0.0f, 0.0f, 0.0f}; // 欧拉角（度），绕 X/Y/Z 轴
    QVector3D scale{1.0f, 1.0f, 1.0f};

    TransformComponent() = default;
    TransformComponent(QVector3D pos, QVector3D rot, QVector3D scl)
        : position(pos), rotation(rot), scale(scl) {}

    // 生成 Model 矩阵（TRS 顺序：Scale → Rotate → Translate）
    QMatrix4x4 getModelMatrix() const
    {
        QMatrix4x4 model;
        model.setToIdentity();
        model.translate(position);
        model.rotate(rotation.x(), 1.0f, 0.0f, 0.0f); // 绕 X 轴
        model.rotate(rotation.y(), 0.0f, 1.0f, 0.0f); // 绕 Y 轴
        model.rotate(rotation.z(), 0.0f, 0.0f, 1.0f); // 绕 Z 轴
        model.scale(scale);
        return model;
    }

    // 获取法线变换矩阵（去除平移分量，再逆转置）
    // 仅适用于均匀缩放 + 旋转的情况
    QMatrix3x3 getNormalMatrix() const
    {
        QMatrix4x4 model = getModelMatrix();
        return model.normalMatrix(); // Qt 提供的法线矩阵（已做逆转置）
    }
};

#endif // ENGINE_COMPONENTS_TRANSFORM_H
