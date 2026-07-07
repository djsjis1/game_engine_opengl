#ifndef CAMERAINPUT_H
#define CAMERAINPUT_H

// ============================================================
// CameraInput — 每帧从窗口采集的输入状态（纯数据）
//
// MainWindow 在按键/鼠标事件中更新此结构，
// CameraSystem 读取它来驱动摄像机。
// ============================================================
struct CameraInput
{
    bool forward = false;  // W
    bool backward = false; // S
    bool left = false;     // A
    bool right = false;    // D
    bool up = false;       // Q
    bool down = false;     // E

    float mouseDX = 0.0f; // 鼠标水平移动（每帧累加）
    float mouseDY = 0.0f; // 鼠标垂直移动

    bool rightMouseDown = false; // 右键按下时旋转视角

    /// 每帧开始前调用，清除瞬时输入
    void resetFrame()
    {
        mouseDX = 0.0f;
        mouseDY = 0.0f;
    }
};

#endif
