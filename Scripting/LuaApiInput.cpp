// ============================================================
// LuaApiInput.cpp — 输入类脚本 API（键盘/鼠标/动作映射/光标/时间）
//
// 全部直读 Input/Time 单例（消费者走接口，不碰内部布局）。
// 拆分地图见 LuaApiInternal.h；加新输入 API：实现写这里 + 主表加一行。
// ============================================================
#include "LuaApiInternal.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include <cstring>

/// delta() → dt（帧间隔，秒）
int api_delta(lua_State *L)
{
    lua_pushnumber(L, Time::instance().delta);
    return 1;
}

/// is_key_pressed("键名") → true/false（支持 "W"/"space"/"left"/"shift"...）
int api_isKeyPressed(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isDown(Input::keyFromName(k)));
    return 1;
}

/// is_key_just_pressed("key") → 本帧刚按下（边沿，跳跃/开枪用）
int api_isKeyJustPressed(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isJustPressed(Input::keyFromName(k)));
    return 1;
}

/// is_key_just_released("key") → 本帧刚松开（边沿）
int api_isKeyJustReleased(lua_State *L)
{
    const char *k = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isJustReleased(Input::keyFromName(k)));
    return 1;
}

/// is_action_pressed("jump") → 动作是否按住（经动作映射，键位可配）
int api_isActionPressed(lua_State *L)
{
    const char *a = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isActionDown(a));
    return 1;
}

/// is_action_just_pressed("jump") → 动作本帧刚触发（边沿）
int api_isActionJustPressed(lua_State *L)
{
    const char *a = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isActionJustPressed(a));
    return 1;
}

// ---------------- 鼠标（对标 Godot Input） ----------------

/// is_mouse_pressed("left"/"middle"/"right") → 鼠标键是否按住（持续）
int api_isMousePressed(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseDown(Input::mouseFromName(b)));
    return 1;
}

/// is_mouse_just_pressed("left") → 本帧刚按下（边沿，点击/射击用）
int api_isMouseJustPressed(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseJustPressed(Input::mouseFromName(b)));
    return 1;
}

/// is_mouse_just_released("left") → 本帧刚松开（边沿，拖拽结束用）
int api_isMouseJustReleased(lua_State *L)
{
    const char *b = luaL_checkstring(L, 1);
    lua_pushboolean(L, Input::instance().isMouseJustReleased(Input::mouseFromName(b)));
    return 1;
}

/// get_mouse_position() → x, y（视口内像素坐标，配合 raycast 拾取）
int api_getMousePosition(lua_State *L)
{
    lua_pushnumber(L, Input::instance().mouseX());
    lua_pushnumber(L, Input::instance().mouseY());
    return 2;
}

/// get_mouse_delta() → dx, dy（本帧相对位移，FPS 视角控制用）
int api_getMouseDelta(lua_State *L)
{
    lua_pushnumber(L, Input::instance().mouseDeltaX());
    lua_pushnumber(L, Input::instance().mouseDeltaY());
    return 2;
}

/// get_mouse_wheel() → delta（本帧滚轮格数，上正下负；缩放/切武器用）
int api_getMouseWheel(lua_State *L)
{
    lua_pushnumber(L, Input::instance().wheel());
    return 1;
}

/// set_mouse_mode("captured"/"visible") — 光标模式（Godot MOUSE_MODE 同款）
///   captured：隐藏光标+锁视口中心（FPS 视角）；visible：恢复正常
///   仅 Play 模式生效（编辑模式强制正常光标）；实际应用由 MainWindow 每帧执行
int api_setMouseMode(lua_State *L)
{
    const char *m = luaL_checkstring(L, 1);
    Input::instance().setMouseMode(strcmp(m, "captured") == 0 ? 1 : 0);
    return 0;
}
