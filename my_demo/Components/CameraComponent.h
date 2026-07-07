#ifndef CAMERACOMPONENT_H
#define CAMERACOMPONENT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================
// CameraComponent — 摄像机数据
//
// 当实体挂上此组件时，CameraSystem 每帧根据输入更新其
// view / projection 矩阵，RenderWidget 读取此矩阵渲染场景。
// ============================================================
struct CameraComponent
{
    glm::vec3 position{0.0f, 3.0f, 8.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 worldUp{0.0f, 1.0f, 0.0f};

    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    float yaw = -90.0f; // 水平角（度）
    float pitch = 0.0f; // 俯仰角（度）
    float moveSpeed = 5.0f;
    float mouseSensitivity = 0.1f;

    bool active = true;    // 当前渲染用的摄像机
    bool isEditor = false; // true=编辑器自由摄像机（响应 WASD/鼠标）
                           // false=场景中放置的游戏摄像机（不响应输入）

    // 每帧由 CameraSystem 更新的矩阵
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projMatrix{1.0f};

    /// 根据 aspect 更新 projMatrix
    void updateProjection(float aspectRatio)
    {
        projMatrix = glm::perspective(
            glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    /// 根据 yaw/pitch 更新 front 向量和 viewMatrix
    void updateView()
    {
        // 限制俯仰角，避免翻转
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);

        viewMatrix = glm::lookAt(position, position + front, up);
    }

    /// WASD 移动
    void moveForward(float dt) { position += front * moveSpeed * dt; }
    void moveBackward(float dt) { position -= front * moveSpeed * dt; }
    void moveLeft(float dt) { position -= glm::normalize(glm::cross(front, up)) * moveSpeed * dt; }
    void moveRight(float dt) { position += glm::normalize(glm::cross(front, up)) * moveSpeed * dt; }
    void moveUp(float dt) { position += up * moveSpeed * dt; }
    void moveDown(float dt) { position -= up * moveSpeed * dt; }

    /// 鼠标旋转
    void rotate(float deltaYaw, float deltaPitch)
    {
        yaw += deltaYaw * mouseSensitivity;
        pitch += deltaPitch * mouseSensitivity;
    }
};

#endif
