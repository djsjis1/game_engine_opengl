// ============================================================
// Easing 单元测试 — 全部缓动函数的数学性质
//   端点约束：f(0)=0, f(1)=1（动画不跳变的根本保证）
//   单调性：Linear/Quad 等基础曲线必须单调（Bounce/Elastic 除外）
// ============================================================
#include <gtest/gtest.h>
#include "Core/Easing.h"

// 全部内置缓动（不含 kCustomBezier——它要额外 4 参数）都必须满足端点约束
TEST(EasingTest, Ease_AllBuiltinTypes_StartAtZeroEndAtOne)
{
    for (int i = 0; i < (int)EasingType::kCustomBezier; ++i)
    {
        EXPECT_NEAR(ease((EasingType)i, 0.0f), 0.0f, 1e-4f) << "type=" << i;
        EXPECT_NEAR(ease((EasingType)i, 1.0f), 1.0f, 1e-4f) << "type=" << i;
    }
}

TEST(EasingTest, Ease_Linear_IsIdentity)
{
    EXPECT_FLOAT_EQ(ease(EasingType::kLinear, 0.25f), 0.25f);
    EXPECT_FLOAT_EQ(ease(EasingType::kLinear, 0.75f), 0.75f);
}

TEST(EasingTest, Ease_InQuad_IsTSquared)
{
    EXPECT_NEAR(ease(EasingType::kEaseInQuad, 0.5f), 0.25f, 1e-5f);
}

TEST(EasingTest, Ease_MonotonicTypes_NeverDecrease)
{
    // Bounce/Elastic 允许过冲回落，其余基础曲线必须单调不减
    for (int type = 0; type <= (int)EasingType::kEaseInOutSine; ++type)
    {
        float prev = 0.0f;
        for (int i = 1; i <= 20; ++i)
        {
            float v = ease((EasingType)type, i / 20.0f);
            EXPECT_GE(v, prev - 1e-5f) << "type=" << type << " t=" << i / 20.0f;
            prev = v;
        }
    }
}

TEST(EasingTest, Ease_IntOverload_MatchesEnumVersion)
{
    EXPECT_FLOAT_EQ(ease(1, 0.5f), ease(EasingType::kEaseInQuad, 0.5f));
}

TEST(EasingTest, EaseBezier_StraightControlPoints_ApproximatesLinear)
{
    // 控制点在对角线上 → 曲线退化为线性
    EXPECT_NEAR(easeBezier(0.25f, 0.25f, 0.75f, 0.75f, 0.5f), 0.5f, 0.01f);
}

TEST(EasingTest, EaseBezier_Endpoints_ZeroAndOne)
{
    EXPECT_NEAR(easeBezier(0.42f, 0.0f, 0.58f, 1.0f, 0.0f), 0.0f, 1e-3f);
    EXPECT_NEAR(easeBezier(0.42f, 0.0f, 0.58f, 1.0f, 1.0f), 1.0f, 1e-3f);
}
