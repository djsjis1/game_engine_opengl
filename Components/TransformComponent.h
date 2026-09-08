#ifndef TRANSFORMCOMPONENT_H
#define TRANSFORMCOMPONENT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components/Reflect.h"

// ============================================================
// TransformComponent — 位置/旋转/缩放
//
// 【父子层级】
//   position/rotation/scale 是【局部】值（相对父实体）
//   worldMatrix 是【世界】变换（TransformSystem 每帧计算）：
//     无父实体：worldMatrix = 局部矩阵
//     有父实体：worldMatrix = 父.worldMatrix × 局部矩阵
//   → 父实体移动，所有子实体自动跟随（角色拿武器、车带轮子）
// ============================================================
struct TransformComponent
{
    REFLECT glm::vec3 position{0.0f, 0.0f, 0.0f};
    REFLECT glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    REFLECT glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // 可见性（对标 Godot Node3D.visible）
    //   visible       : 用户设置的可见性（序列化）
    //   visibleInTree : 运行时有效可见性 = 父.visibleInTree && 本.visible
    //                   （TransformSystem 每帧计算，父隐藏则子也隐藏，不序列化）
    REFLECT bool visible = true;
    bool visibleInTree = true;

    // 局部矩阵缓存（updateMatrices 计算）
    glm::mat4 modelMatrix{1.0f};
    glm::mat3 normalMatrix{1.0f};

    // 世界矩阵缓存（TransformSystem 每帧计算，含父级变换）
    glm::mat4 worldMatrix{1.0f};
    glm::mat3 worldNormalMatrix{1.0f};

    /// 重算局部矩阵（position/rotation/scale → modelMatrix）
    void updateMatrices()
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        modelMatrix = m;
        normalMatrix = glm::mat3(glm::transpose(glm::inverse(m)));
    }
};

#endif
