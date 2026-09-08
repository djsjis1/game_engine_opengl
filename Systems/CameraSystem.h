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
            // ★ 矩阵计算对所有相机，输入控制只对"激活的编辑器相机"：
            //   Play 模式下 MainWindow 会把编辑器相机 active=false，
            //   它就不再吃 WASD——否则玩家按 W 动的是相机不是角色（实踩坑）
            if (cam.isEditor && cam.active)
            {
                // ↓↓↓ 编辑器飞行相机：输入驱动 ↓↓↓

                // 1. 根据 yaw/pitch 计算 front/right 向量
                float yr = glm::radians(cam.yaw);
                float pr = glm::radians(cam.pitch);
                glm::vec3 front(cos(yr) * cos(pr), sin(pr), sin(yr) * cos(pr));
                front = glm::normalize(front);
                glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

                // ★ WASD/QE 飞行移动：编辑模式始终有效（对标 Godot/Unity 编辑器：
                //   自由飞行不需要按住任何键）。鼠标旋转才需要按住右键——
                //   否则左键拾取/拖 Gizmo 时会误转视角。
                //   （旧设计曾把移动也锁在右键下：用户反馈“按住 WASD 视角不动”
                //    反人类，实踩坑）
                float speed = cam.moveSpeed * dt;
                if (in.isDown('W')) cam.position += front * speed;
                if (in.isDown('S')) cam.position -= front * speed;
                if (in.isDown('A')) cam.position -= right * speed;
                if (in.isDown('D')) cam.position += right * speed;
                if (in.isDown('Q')) cam.position.y += speed;
                if (in.isDown('E')) cam.position.y -= speed;

                // 鼠标旋转：按住右键才转（左键留给拾取/Gizmo 拖动）
                if (in.isMouseDown(MOUSE_RIGHT))
                {
                    cam.yaw += in.mouseDeltaX() * cam.mouseSens;
                    cam.pitch -= in.mouseDeltaY() * cam.mouseSens;
                    cam.pitch = glm::clamp(cam.pitch, -89.0f, 89.0f);
                }
            }

            // 4. 用最新 yaw/pitch 算 front → view 矩阵（所有相机都要）
            //    注：proj 矩阵由 RenderWidget 按实际窗口宽高比每帧重算
            float yr = glm::radians(cam.yaw);
            float pr = glm::radians(cam.pitch);
            glm::vec3 front = glm::normalize(
                glm::vec3(cos(yr)*cos(pr), sin(pr), sin(yr)*cos(pr)));
            cam.viewMatrix = glm::lookAt(cam.position, cam.position + front,
                                         glm::vec3(0, 1, 0)); });
    }
};

#endif
