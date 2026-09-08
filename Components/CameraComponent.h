#ifndef CAMERACOMPONENT_H
#define CAMERACOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"

// ============================================================
// CameraComponent —— 摄像机纯数据
//
// isEditor = true  → CameraSystem 每帧根据 Input 驱动此摄像机（WASD+鼠标）
// active   = true  → RenderWidget 使用此摄像机的 view/proj 渲染
//
// 编辑器摄像机：isEditor=true, active=true
// 游戏摄像机：  isEditor=false, active=true
// ============================================================
struct CameraComponent
{
    // REFLECT 标记的字段由 reflect_gen 自动生成反射注册（编辑器/序列化自动支持）
    REFLECT glm::vec3 position{0, 3, 8};
    REFLECT float yaw = -90;
    REFLECT float pitch = 0;
    REFLECT float fov = 45;
    REFLECT float nearPlane = 0.1f;
    REFLECT float farPlane = 100;
    REFLECT float moveSpeed = 5;
    REFLECT float mouseSens = 0.1f;

    REFLECT bool isEditor = false; // CameraSystem 是否驱动此摄像机
    REFLECT bool active = true;    // RenderWidget 是否使用此摄像机渲染

    // 运行时缓存（不打标记 = 反射不可见，编辑器/存档都不碰）
    glm::mat4 viewMatrix{1};
};

#endif
