#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <cstring>
#include <string>
#include <unordered_map>

// ============================================================
// Input — 引擎全局输入单例（Godot 风格）
//
// 【解耦边界】数据全部 private，外界只能走两组接口：
//   写入口（仅 MainWindow 事件层调用）：onKey/onMouseButton/onMousePosition/
//                                       onWheel/onMouseMove/endFrame
//   读接口（系统/Lua 消费者调用）：isDown/isJustPressed/isMouse*/
//                                  mousePosition/mouseDelta/wheel/isAction*
// 内部存储布局（数组/映射方式）以后随便改，接口不动 → 消费者零改动。
// （教训：以前 CameraSystem 直接读 keyDown[]/mouseRight，内部一改全跟着改）
//
// 【设计要点】
//  ① 键码归一化到 [0,256)：字母/数字用 ASCII（<128），
//     方向键/空格/Shift/F 键等特殊键映射到 128+ 区间（见 normalizeKey）
//  ② 三态：Down（持续按住）+ JustPressed（本帧刚按下）+ JustReleased（刚松开）
//     键盘/鼠标同构，边沿检测供跳跃/开枪等一次性操作
//  ③ 动作映射：isActionDown("jump") 而非硬编码物理键（Godot 同款，键位可配置）
// ============================================================

// 归一化键码常量（特殊键放 128+ 区间，避开 ASCII）
enum : int
{
    KEY_SPACE = 128,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_ENTER,
    KEY_ESC,
    KEY_TAB,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_COUNT // 必须 < 256
};

// 鼠标键下标（isMouseDown 等接口的参数）
enum : int
{
    MOUSE_LEFT = 0,
    MOUSE_MIDDLE = 1,
    MOUSE_RIGHT = 2,
};

class Input
{
public:
    static Input &instance()
    {
        static Input s;
        return s;
    }

    // ================= 写入口（仅事件层 MainWindow 调用） =================

    void onKey(int qtKey, bool pressed)
    {
        int c = normalizeKey(qtKey);
        if (c < 0 || c >= 256)
            return; // 未映射键忽略
        if (pressed && !m_keyDown[c])
            m_keyPressed[c] = true; // 边沿：刚按下
        if (!pressed && m_keyDown[c])
            m_keyReleased[c] = true; // 边沿：刚松开
        m_keyDown[c] = pressed;
    }

    // 鼠标按键三态（btn 用 MOUSE_LEFT/MIDDLE/RIGHT），与 onKey 同构
    void onMouseButton(int btn, bool pressed)
    {
        if (btn < 0 || btn >= 3)
            return;
        if (pressed && !m_mouseDown[btn])
            m_mousePressed[btn] = true;
        if (!pressed && m_mouseDown[btn])
            m_mouseReleased[btn] = true;
        m_mouseDown[btn] = pressed;
    }

    void onMousePosition(float x, float y)
    {
        m_mouseX = x;
        m_mouseY = y;
    }

    void onMouseMove(float dx, float dy)
    {
        m_mouseDX += dx;
        m_mouseDY += dy;
    }

    void onWheel(float delta) { m_wheelDelta += delta; }

    // 帧末清理一次性事件（边沿/增量）
    void endFrame()
    {
        memset(m_keyPressed, 0, sizeof(m_keyPressed));
        memset(m_keyReleased, 0, sizeof(m_keyReleased));
        memset(m_mousePressed, 0, sizeof(m_mousePressed));
        memset(m_mouseReleased, 0, sizeof(m_mouseReleased));
        m_mouseDX = 0;
        m_mouseDY = 0;
        m_wheelDelta = 0; // 滚轮是帧内增量，下帧归零
    }

    /// 全量重置（退出 Play 清输入残留 / 单元测试隔离用）：
    /// 按住的键、边沿、鼠标、光标模式全部回初始态；动作映射保留（配置不丢）
    void reset()
    {
        memset(m_keyDown, 0, sizeof(m_keyDown));
        memset(m_mouseDown, 0, sizeof(m_mouseDown));
        endFrame();
        m_mouseX = m_mouseY = 0;
        m_mouseMode = kMouseVisible;
    }

    // ================= 读接口（系统/Lua 消费者调用） =================

    // ---- 键盘（key 用归一化键码；'W' 这种字母直接传 ASCII）----
    bool isDown(int key) const { return valid(key) && m_keyDown[key]; }
    bool isJustPressed(int key) const { return valid(key) && m_keyPressed[key]; }
    bool isJustReleased(int key) const { return valid(key) && m_keyReleased[key]; }

    // ---- 鼠标（btn 用 MOUSE_LEFT/MIDDLE/RIGHT）----
    bool isMouseDown(int b) const { return (b >= 0 && b < 3) && m_mouseDown[b]; }
    bool isMouseJustPressed(int b) const { return (b >= 0 && b < 3) && m_mousePressed[b]; }
    bool isMouseJustReleased(int b) const { return (b >= 0 && b < 3) && m_mouseReleased[b]; }

    float mouseX() const { return m_mouseX; } // 视口内像素坐标
    float mouseY() const { return m_mouseY; }
    float mouseDeltaX() const { return m_mouseDX; } // 本帧相对位移（相机转视角用）
    float mouseDeltaY() const { return m_mouseDY; }
    float wheel() const { return m_wheelDelta; } // 本帧滚轮增量（一格=±1）

    // ---- 动作映射（按动作名，经 actionMap 转键码）----
    bool isActionDown(const std::string &a) const { return isDown(actionKey(a)); }
    bool isActionJustPressed(const std::string &a) const { return isJustPressed(actionKey(a)); }

    /// 运行时改键位（如设置界面）：bindAction("jump", KEY_SPACE)
    void bindAction(const std::string &action, int key) { m_actionMap[action] = key; }

    // ---- 光标模式（对标 Godot MOUSE_MODE_*）----
    // Input 只存"期望模式"（脚本写）；隐藏光标/锁中心由 MainWindow
    // 每帧读取应用（UI 层的事，Input 不碰 Qt —— 层间解耦）
    enum MouseMode : int
    {
        kMouseVisible = 0,  // 正常（默认）
        kMouseCaptured = 1, // 隐藏+锁视口中心（FPS 视角）
    };
    void setMouseMode(int m) { m_mouseMode = (m == 1) ? kMouseCaptured : kMouseVisible; }
    int mouseMode() const { return m_mouseMode; }

    // ================= 键码换算（静态工具） =================

    // Qt 键码 → 归一化键码（字母大写 ASCII / 特殊键映射到 128+）
    // 修复原 (char)qtKey 截断：Qt::Key_Left=0x01000012 截断后变 0x12 冲突
    static int normalizeKey(int qtKey)
    {
        if (qtKey >= 'A' && qtKey <= 'Z')
            return qtKey;
        if (qtKey >= 'a' && qtKey <= 'z')
            return qtKey - 32;
        if (qtKey >= '0' && qtKey <= '9')
            return qtKey;
        switch (qtKey)
        {
        case 0x20:
            return KEY_SPACE; // Qt::Key_Space
        case 0x01000012:
            return KEY_LEFT; // Qt::Key_Left
        case 0x01000014:
            return KEY_RIGHT; // Qt::Key_Right
        case 0x01000013:
            return KEY_UP; // Qt::Key_Up
        case 0x01000015:
            return KEY_DOWN; // Qt::Key_Down
        case 0x01000020:
            return KEY_SHIFT; // Qt::Key_Shift
        case 0x01000021:
            return KEY_CTRL; // Qt::Key_Control
        case 0x01000023:
            return KEY_ALT; // Qt::Key_Alt
        case 0x01000004:
            return KEY_ENTER; // Qt::Key_Return
        case 0x01000005:
            return KEY_ENTER; // Qt::Key_Enter（小键盘）
        case 0x01000000:
            return KEY_ESC; // Qt::Key_Escape
        case 0x01000001:
            return KEY_TAB; // Qt::Key_Tab
        case 0x01000030:
            return KEY_F1;
        case 0x01000031:
            return KEY_F2;
        case 0x01000032:
            return KEY_F3;
        case 0x01000033:
            return KEY_F4;
        default:
            return -1; // 未映射的键忽略
        }
    }

    // 字符串键名 → 归一化键码（Lua 端 is_key_pressed("W")/("space") 用）
    static int keyFromName(const char *name)
    {
        if (!name || !name[0])
            return -1;
        if (!name[1]) // 单字符：字母/数字
        {
            char c = name[0];
            if (c >= 'a' && c <= 'z')
                c -= 32;
            return (int)(unsigned char)c;
        }
        std::string s(name);
        for (auto &ch : s)
            if (ch >= 'A' && ch <= 'Z')
                ch += 32; // 转小写统一比较
        if (s == "space")
            return KEY_SPACE;
        if (s == "left")
            return KEY_LEFT;
        if (s == "right")
            return KEY_RIGHT;
        if (s == "up")
            return KEY_UP;
        if (s == "down")
            return KEY_DOWN;
        if (s == "shift")
            return KEY_SHIFT;
        if (s == "ctrl")
            return KEY_CTRL;
        if (s == "alt")
            return KEY_ALT;
        if (s == "enter")
            return KEY_ENTER;
        if (s == "esc")
            return KEY_ESC;
        if (s == "tab")
            return KEY_TAB;
        if (s == "f1")
            return KEY_F1;
        if (s == "f2")
            return KEY_F2;
        if (s == "f3")
            return KEY_F3;
        if (s == "f4")
            return KEY_F4;
        return -1;
    }

    // 鼠标键名 → 下标（Lua 端 is_mouse_pressed("left") 用）
    static int mouseFromName(const char *name)
    {
        if (!name || !name[0])
            return -1;
        std::string s(name);
        for (auto &ch : s)
            if (ch >= 'A' && ch <= 'Z')
                ch += 32;
        if (s == "left")
            return MOUSE_LEFT;
        if (s == "middle")
            return MOUSE_MIDDLE;
        if (s == "right")
            return MOUSE_RIGHT;
        return -1;
    }

private:
    Input() { setupDefaultActions(); }

    static bool valid(int key) { return key >= 0 && key < 256; }

    int actionKey(const std::string &action) const
    {
        auto it = m_actionMap.find(action);
        return it == m_actionMap.end() ? -1 : it->second;
    }

    // 默认动作（对标 Godot 默认输入映射）
    void setupDefaultActions()
    {
        m_actionMap["jump"] = KEY_SPACE;
        m_actionMap["move_left"] = 'A';
        m_actionMap["move_right"] = 'D';
        m_actionMap["move_forward"] = 'W';
        m_actionMap["move_back"] = 'S';
        m_actionMap["sprint"] = KEY_SHIFT;
        m_actionMap["ui_accept"] = KEY_ENTER;
        m_actionMap["ui_cancel"] = KEY_ESC;
    }

    // ---- 全部私有：布局以后随便改，接口不动 ----
    bool m_keyDown[256] = {};
    bool m_keyPressed[256] = {};
    bool m_keyReleased[256] = {};
    bool m_mouseDown[3] = {};
    bool m_mousePressed[3] = {};
    bool m_mouseReleased[3] = {};
    float m_mouseX = 0, m_mouseY = 0;
    float m_mouseDX = 0, m_mouseDY = 0;
    float m_wheelDelta = 0;
    int m_mouseMode = kMouseVisible;
    std::unordered_map<std::string, int> m_actionMap;
};

#endif
