#ifndef CAMERASYSTEM_H
#define CAMERASYSTEM_H

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Core/Input.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================
// CameraSystem —— 摄像机逻辑（纯 ECS System）
//
// 遍历 isEditor=true 的 CameraComponent，根据 Input 驱动。
// 所有摄像机逻辑（移动、旋转、矩阵计算）在此处，不在 Component 里。
// 同时计算 view/proj 矩阵供 RenderWidget 读取。
// ============================================================
class CameraSystem : public System
{
public:
    int phase() const override { return 50; }            // 最先执行：更新摄像机矩阵
    int activeIn() const override { return kAllStates; } // 编辑和运行都需要

    void update(flecs::world &world, float dt) override
    {
        auto &in = Input::instance();

        world.each([&](CameraComponent &cam)
                   {
            if (!cam.isEditor) return;

            // ↓↓↓ 所有逻辑在这里，Component 只是数据 ↓↓↓

            // 1. 根据 yaw/pitch 计算 front/right 向量
            float yr = glm::radians(cam.yaw);
            float pr = glm::radians(cam.pitch);
            glm::vec3 front(cos(yr) * cos(pr), sin(pr), sin(yr) * cos(pr));
            front = glm::normalize(front);
            glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
            glm::vec3 up(0, 1, 0);

            // 2. WASD 移动
            float speed = cam.moveSpeed * dt;
            if (in.keyDown['W']) cam.position += front * speed;
            if (in.keyDown['S']) cam.position -= front * speed;
            if (in.keyDown['A']) cam.position -= right * speed;
            if (in.keyDown['D']) cam.position += right * speed;
            if (in.keyDown['Q']) cam.position.y += speed;
            if (in.keyDown['E']) cam.position.y -= speed;

            // 3. 鼠标旋转
            if (in.mouseRight) {
                cam.yaw   += in.mouseDX * cam.mouseSens;
                cam.pitch -= in.mouseDY * cam.mouseSens;
                cam.pitch  = glm::clamp(cam.pitch, -89.0f, 89.0f);
            }

            // 4. 重算 front（yaw/pitch 变了）
            yr = glm::radians(cam.yaw);
            pr = glm::radians(cam.pitch);
            front = glm::normalize(glm::vec3(cos(yr)*cos(pr), sin(pr), sin(yr)*cos(pr)));

            // 5. 更新 view 矩阵（供 RenderWidget 读取）
            //    注：proj 矩阵由 RenderWidget 根据实际窗口宽高比每帧重算，
            //    这里不算（系统不知道窗口尺寸）
            cam.viewMatrix = glm::lookAt(cam.position, cam.position + front, up); });
    }
};

#endif
