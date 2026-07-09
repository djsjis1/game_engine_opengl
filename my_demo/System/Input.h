#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <cstring>

// ============================================================
// Input — 引擎全局输入单例（Godot 风格）
// MainWindow 写、ScriptSystem/EditorCamera 读
// ============================================================
struct Input
{
    static Input &instance()
    {
        static Input s;
        return s;
    }

    bool keyDown[256] = {};
    bool keyPressed[256] = {};
    bool keyReleased[256] = {};
    float mouseDX = 0, mouseDY = 0;
    bool mouseRight = false;

    void endFrame()
    {
        memset(keyPressed, 0, sizeof(keyPressed));
        memset(keyReleased, 0, sizeof(keyReleased));
        mouseDX = 0;
        mouseDY = 0;
    }

    void onKey(int qtKey, bool pressed)
    {
        char c = (char)qtKey;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (pressed && !keyDown[(unsigned char)c])
            keyPressed[(unsigned char)c] = true;
        if (!pressed && keyDown[(unsigned char)c])
            keyReleased[(unsigned char)c] = true;
        keyDown[(unsigned char)c] = pressed;
    }
    void onMouseMove(float dx, float dy)
    {
        mouseDX += dx;
        mouseDY += dy;
    }
    void onMousePress(bool r) { mouseRight = r; }
    void onMouseRelease(bool) { mouseRight = false; }
};

#endif
