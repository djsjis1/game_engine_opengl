// ============================================================
// Time 单元测试 — delta / 螺旋死亡钳制 / 固定步长追赶 / 插值系数
// 隔离：Time 是 struct，直接构造局部实例（不用单例，天然隔离）
// ============================================================
#include <gtest/gtest.h>
#include "Core/Time.h"

TEST(TimeTest, Step_NormalDt_SetsDeltaAndAccumulatesTotal)
{
    Time t;
    t.step(0.016f);
    EXPECT_FLOAT_EQ(t.delta, 0.016f);
    t.step(0.020f);
    EXPECT_FLOAT_EQ(t.delta, 0.020f);
    EXPECT_FLOAT_EQ(t.totalTime, 0.036f);
    EXPECT_EQ(t.frameCount, 2u);
}

TEST(TimeTest, Step_HugeDt_ClampedToQuarterSecond)
{
    Time t;
    t.step(5.0f);                    // 断点/卡死后的巨帧
    EXPECT_FLOAT_EQ(t.delta, 0.25f); // 螺旋死亡保护上限
}

TEST(TimeTest, ConsumePhysicsStep_OneFrameAt60fps_YieldsOneStep)
{
    Time t;
    t.step(1.0f / 60.0f + 0.0001f); // 略超一个物理步长
    EXPECT_TRUE(t.consumePhysicsStep());
    EXPECT_FALSE(t.consumePhysicsStep()); // 步数用完
}

TEST(TimeTest, ConsumePhysicsStep_BigFrame_CatchesUpMultipleSteps)
{
    Time t;
    t.step(3.5f / 60.0f); // 一帧顶 3.5 个物理步 → 追赶 3 步
    int steps = 0;
    while (t.consumePhysicsStep())
        steps++;
    EXPECT_EQ(steps, 3);
}

TEST(TimeTest, Interp_HalfStepRemainder_ReturnsHalf)
{
    Time t;
    t.step(1.5f / 60.0f);                 // 1 步整 + 半步余量
    EXPECT_NEAR(t.interp(), 0.5f, 1e-4f); // 余量/步长 = 0.5
}

TEST(TimeTest, Interp_Always_InZeroToOneRange)
{
    Time t;
    for (int i = 0; i < 100; ++i)
    {
        t.step(0.017f); // 不整除的帧长
        EXPECT_GE(t.interp(), 0.0f);
        EXPECT_LT(t.interp(), 1.0f);
    }
}
