// ============================================================
// SceneRouter 单元测试 — 延迟切场景语义 / 当前场景 / Play 期变更标记
// 隔离：单例，SetUp 用公开接口清空全部状态
// ============================================================
#include <gtest/gtest.h>
#include "Core/SceneRouter.h"

class SceneRouterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto &r = SceneRouter::instance();
        r.takePending(); // 清残留请求
        r.setCurrent("");
        r.resetChanged();
    }
    SceneRouter &router = SceneRouter::instance();
};

TEST_F(SceneRouterTest, RequestChange_SetsPending)
{
    EXPECT_FALSE(router.hasPending());
    router.requestChange("scenes/level2.json");
    EXPECT_TRUE(router.hasPending());
}

TEST_F(SceneRouterTest, TakePending_ReturnsPathAndClears)
{
    router.requestChange("scenes/level2.json");
    EXPECT_EQ(router.takePending(), "scenes/level2.json");
    EXPECT_FALSE(router.hasPending()); // 取完即清（防重复加载）
}

TEST_F(SceneRouterTest, RequestChange_Twice_LastOneWins)
{
    router.requestChange("scenes/a.json");
    router.requestChange("scenes/b.json"); // 同帧两次请求
    EXPECT_EQ(router.takePending(), "scenes/b.json");
}

TEST_F(SceneRouterTest, TakePending_NoRequest_ReturnsEmpty)
{
    EXPECT_EQ(router.takePending(), "");
}

TEST_F(SceneRouterTest, SetCurrent_GetCurrent_RoundTrip)
{
    router.setCurrent("scenes/level1.json");
    EXPECT_EQ(router.current(), "scenes/level1.json");
}

TEST_F(SceneRouterTest, MarkChanged_ResetChanged_FlagLifecycle)
{
    EXPECT_FALSE(router.changedInPlay());
    router.markChanged();
    EXPECT_TRUE(router.changedInPlay());
    router.resetChanged();
    EXPECT_FALSE(router.changedInPlay());
}
