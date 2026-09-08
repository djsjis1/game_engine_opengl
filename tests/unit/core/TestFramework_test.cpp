// ============================================================
// TestFramework_test.cpp — 引擎自检/统计框架单元测试
// 命名：TEST(类名Test, 方法名_场景_期望结果)
// ============================================================
#include "Core/TestFramework.h"
#include <gtest/gtest.h>

// ---- ET_CHECK 断言宏：通过/失败/关闭三态 ----

TEST(TestFrameworkTest, ETCHECK_TrueExpression_ReturnsTrue)
{
    EXPECT_TRUE(ET_CHECK(1 + 1 == 2, "basic arithmetic"));
    EXPECT_EQ(EngineTest::stats().checkPasses.load(), 1u);
}

TEST(TestFrameworkTest, ETCHECK_FalseExpression_ReturnsFalseAndCounts)
{
    uint64_t before = EngineTest::stats().checkFailures.load();
    EXPECT_FALSE(ET_CHECK(1 + 1 == 3, "intentional failure"));
    EXPECT_EQ(EngineTest::stats().checkFailures.load(), before + 1);
}

// ---- 统计计数器：atomic 无锁读写 ----

TEST(TestFrameworkTest, Counters_IncrementAndReset_CorrectValues)
{
    EngineTest::stats().drawCalls.store(0);
    EngineTest::stats().uniformSets.store(0);

    EngineTest::countDrawCall();
    EngineTest::countDrawCall();
    EngineTest::countUniform();

    EXPECT_EQ(EngineTest::stats().drawCalls.load(), 2u);
    EXPECT_EQ(EngineTest::stats().uniformSets.load(), 1u);

    // beginFrame 归零"本帧"计数
    EngineTest::beginFrame();
    EXPECT_EQ(EngineTest::stats().drawCalls.load(), 0u);
    EXPECT_EQ(EngineTest::stats().uniformSets.load(), 0u);
}

TEST(TestFrameworkTest, BeginFrame_KeepsCumulativeFrameCounter)
{
    EngineTest::stats().frameCounter.store(0);
    ++EngineTest::stats().frameCounter;                     // 模拟 paintGL 帧计数
    EngineTest::beginFrame();                               // 归零本帧计数
    EXPECT_EQ(EngineTest::stats().frameCounter.load(), 1u); // 累计值保留
}

// ---- 报告生成 ----

TEST(TestFrameworkTest, StatsReport_ContainsExpectedKeys)
{
    std::string report = EngineTest::statsReport();
    EXPECT_NE(report.find("draw calls/frame"), std::string::npos);
    EXPECT_NE(report.find("entities"), std::string::npos);
    EXPECT_NE(report.find("checks pass/fail"), std::string::npos);
}

TEST(TestFrameworkTest, RunSelfCheck_ReturnsPassSummary)
{
    std::string summary = EngineTest::runSelfCheck();
    // 至少包含 PASS 关键字（宏关闭时自检项仍可运行，但 CHECK 恒真）
    EXPECT_NE(summary.find("PASS"), std::string::npos);
}

// ---- 生产构建零开销验证（宏关闭时 ET_CHECK 不评估表达式） ----

#if !ENABLE_ENGINE_TESTING
TEST(TestFrameworkTest, Disabled_ETCHECK_DoesNotEvaluateExpression)
{
    // 若宏展开为 (true)，右侧表达式不会被求值 → 计数器不变
    uint64_t before = EngineTest::stats().checkPasses.load();
    bool r = ET_CHECK(++EngineTest::stats().checkPasses > 0, "side effect");
    EXPECT_TRUE(r);
    // 宏关闭时表达式未求值：checkPasses 只增了 checkImpl 那 0 次
    // （注意：宏关闭时 ET_CHECK 直接展开为 true，不调 checkImpl）
    EXPECT_EQ(EngineTest::stats().checkPasses.load(), before);
}
#endif
