#ifndef CAMERASYSTEM_H
#define CAMERASYSTEM_H

#include "System.h"
#include "../Components/CameraComponent.h"
#include "CameraInput.h"

// ============================================================
// CameraSystem —— 根据输入驱动编辑器摄像机
//
// 只操作 isEditor=true 的 CameraComponent。
// 场景中的游戏摄像机（isEditor=false）不会响应用户输入，
// 保持其位置不变（留给脚本或固定视角）。
// ============================================================
class CameraSystem : public System
{
public:
    /// 绑定外部输入结构（由 MainWindow 每帧提供）
    void setInput(const CameraInput *input) { m_input = input; }

    void update(flecs::world &world, float dt) override
    {
        if (!m_input)
            return;

        // 只遍历编辑器摄像机（isEditor=true），忽略游戏摄像机
        world.each([&](CameraComponent &cam)
                   {
            if (!cam.isEditor) return;  // 跳过游戏摄像机（不响应输入）

            // WASD 移动
            if (m_input->forward)  cam.moveForward(dt);
            if (m_input->backward) cam.moveBackward(dt);
            if (m_input->left)     cam.moveLeft(dt);
            if (m_input->right)    cam.moveRight(dt);
            if (m_input->up)       cam.moveUp(dt);
            if (m_input->down)     cam.moveDown(dt);

            // 鼠标旋转（右键按下时）
            if (m_input->rightMouseDown)
                cam.rotate(m_input->mouseDX, -m_input->mouseDY);

            cam.updateView(); });
    }

private:
    const CameraInput *m_input = nullptr;
};

#endif
