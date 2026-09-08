// ============================================================
// Input 单元测试 — 键码归一化 / 三态边沿 / 鼠标 / 动作映射 / 键名解析
// 隔离：每用例 SetUp 里 reset()（Input 是单例，状态必须清）
// ============================================================
#include <gtest/gtest.h>
#include "Core/Input.h"

class InputTest : public ::testing::Test
{
protected:
    void SetUp() override { Input::instance().reset(); }
    Input &in = Input::instance();
};

// ---- 键码归一化 ----

TEST_F(InputTest, NormalizeKey_LowercaseLetter_ReturnsUppercaseAscii)
{
    EXPECT_EQ(Input::normalizeKey('w'), 'W');
    EXPECT_EQ(Input::normalizeKey('a'), 'A');
}

TEST_F(InputTest, NormalizeKey_QtSpecialKeys_MappedTo128Range)
{
    EXPECT_EQ(Input::normalizeKey(0x20), KEY_SPACE);       // Qt::Key_Space
    EXPECT_EQ(Input::normalizeKey(0x01000012), KEY_LEFT);  // Qt::Key_Left
    EXPECT_EQ(Input::normalizeKey(0x01000020), KEY_SHIFT); // Qt::Key_Shift
    EXPECT_EQ(Input::normalizeKey(0x01000000), KEY_ESC);   // Qt::Key_Escape
}

TEST_F(InputTest, NormalizeKey_UnmappedKey_ReturnsMinusOne)
{
    EXPECT_EQ(Input::normalizeKey(0x01FFFFFF), -1);
}

// ---- 三态（Down / JustPressed / JustReleased）----

TEST_F(InputTest, OnKey_Press_SetsDownAndJustPressed)
{
    in.onKey('W', true);
    EXPECT_TRUE(in.isDown('W'));
    EXPECT_TRUE(in.isJustPressed('W'));
    EXPECT_FALSE(in.isJustReleased('W'));
}

TEST_F(InputTest, EndFrame_ClearsEdgesButKeepsDown)
{
    in.onKey('W', true);
    in.endFrame();
    EXPECT_TRUE(in.isDown('W'));         // 持续按住跨帧保留
    EXPECT_FALSE(in.isJustPressed('W')); // 边沿只活一帧
}

TEST_F(InputTest, OnKey_HoldAcrossFrames_NoRepeatedJustPressed)
{
    in.onKey('W', true);
    in.endFrame();
    in.onKey('W', true);                 // 系统重复键事件（按住不放）
    EXPECT_FALSE(in.isJustPressed('W')); // 不产生第二次边沿
}

TEST_F(InputTest, OnKey_Release_SetsJustReleasedClearsDown)
{
    in.onKey('W', true);
    in.endFrame();
    in.onKey('W', false);
    EXPECT_FALSE(in.isDown('W'));
    EXPECT_TRUE(in.isJustReleased('W'));
}

TEST_F(InputTest, IsDown_OutOfRangeKey_ReturnsFalse)
{
    EXPECT_FALSE(in.isDown(-1));
    EXPECT_FALSE(in.isDown(999));
}

// ---- 鼠标（与键盘同构 + 增量累加）----

TEST_F(InputTest, OnMouseButton_Press_SetsDownAndEdge)
{
    in.onMouseButton(MOUSE_RIGHT, true);
    EXPECT_TRUE(in.isMouseDown(MOUSE_RIGHT));
    EXPECT_TRUE(in.isMouseJustPressed(MOUSE_RIGHT));
}

TEST_F(InputTest, OnMouseMove_MultipleEvents_DeltaAccumulates)
{
    in.onMouseMove(3.f, -2.f);
    in.onMouseMove(1.f, 1.f); // 同帧多次事件累加
    EXPECT_FLOAT_EQ(in.mouseDeltaX(), 4.f);
    EXPECT_FLOAT_EQ(in.mouseDeltaY(), -1.f);
    in.endFrame();
    EXPECT_FLOAT_EQ(in.mouseDeltaX(), 0.f); // 帧末归零
}

TEST_F(InputTest, OnWheel_EndFrame_DeltaResets)
{
    in.onWheel(1.f);
    EXPECT_FLOAT_EQ(in.wheel(), 1.f);
    in.endFrame();
    EXPECT_FLOAT_EQ(in.wheel(), 0.f);
}

// ---- 动作映射 ----

TEST_F(InputTest, IsActionDown_DefaultJumpBinding_MapsToSpace)
{
    in.onKey(0x20, true); // 空格
    EXPECT_TRUE(in.isActionDown("jump"));
    EXPECT_TRUE(in.isActionJustPressed("jump"));
}

TEST_F(InputTest, BindAction_Rebind_NewKeyTakesEffect)
{
    in.bindAction("jump", 'J');
    in.onKey('J', true);
    EXPECT_TRUE(in.isActionDown("jump"));
}

TEST_F(InputTest, IsActionDown_UnknownAction_ReturnsFalse)
{
    EXPECT_FALSE(in.isActionDown("no_such_action"));
}

// ---- 键名解析（Lua 端 is_key_pressed("space") 的底层）----

TEST_F(InputTest, KeyFromName_SingleChar_ReturnsUppercase)
{
    EXPECT_EQ(Input::keyFromName("w"), 'W');
    EXPECT_EQ(Input::keyFromName("W"), 'W');
}

TEST_F(InputTest, KeyFromName_SpecialNamesCaseInsensitive_ReturnsEnum)
{
    EXPECT_EQ(Input::keyFromName("space"), KEY_SPACE);
    EXPECT_EQ(Input::keyFromName("SPACE"), KEY_SPACE);
    EXPECT_EQ(Input::keyFromName("shift"), KEY_SHIFT);
}

TEST_F(InputTest, KeyFromName_InvalidInput_ReturnsMinusOne)
{
    EXPECT_EQ(Input::keyFromName(nullptr), -1);
    EXPECT_EQ(Input::keyFromName(""), -1);
    EXPECT_EQ(Input::keyFromName("nokey"), -1);
}

TEST_F(InputTest, MouseFromName_Names_ReturnIndices)
{
    EXPECT_EQ(Input::mouseFromName("left"), MOUSE_LEFT);
    EXPECT_EQ(Input::mouseFromName("RIGHT"), MOUSE_RIGHT);
    EXPECT_EQ(Input::mouseFromName("bad"), -1);
}

// ---- 光标模式 ----

TEST_F(InputTest, SetMouseMode_InvalidValue_FallsBackToVisible)
{
    in.setMouseMode(1);
    EXPECT_EQ(in.mouseMode(), Input::kMouseCaptured);
    in.setMouseMode(99); // 非法值 → 回落 visible
    EXPECT_EQ(in.mouseMode(), Input::kMouseVisible);
}
