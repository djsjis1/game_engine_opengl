#ifndef EDITORCAMERA_H
#define EDITORCAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Input.h"

// ============================================================
// EditorCamera —— Godot 式编辑器摄像机
//
// 只在编辑模式激活。WASD 移动，右键拖拽旋转。
// 直接从 Input 单例读数据，不依赖 CameraComponent。
// Play 模式下不运行。
// ============================================================
struct EditorCamera
{
    glm::vec3 position{0, 3, 8};
    float yaw = -90, pitch = 0;
    float moveSpeed = 5, mouseSens = 0.1f;

    glm::mat4 viewMatrix{1}, projMatrix{1};

    void update(float dt, float aspect)
    {
        auto &in = Input::instance();

        // 移动
        glm::vec3 front(cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                        sin(glm::radians(pitch)),
                        sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

        float speed = moveSpeed * dt;
        if (in.keyDown['W'])
            position += front * speed;
        if (in.keyDown['S'])
            position -= front * speed;
        if (in.keyDown['A'])
            position -= right * speed;
        if (in.keyDown['D'])
            position += right * speed;
        if (in.keyDown['Q'])
            position.y += speed;
        if (in.keyDown['E'])
            position.y -= speed;

        // 旋转（右键按住时）
        if (in.mouseRight)
        {
            yaw += in.mouseDX * mouseSens;
            pitch -= in.mouseDY * mouseSens;
            pitch = glm::clamp(pitch, -89.0f, 89.0f);
        }

        // 矩阵
        viewMatrix = glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
        projMatrix = glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);
    }
};

#endif
