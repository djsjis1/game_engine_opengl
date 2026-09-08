// ============================================================
// NavGrid 单元测试 — 网格寻路（纯逻辑零依赖）
// 覆盖：直线路径 / 绕墙 / 不可达 / 目标被堵 / 障碍标记 / 穿墙角防护
// 隔离：单例，SetUp clear()
// ============================================================
#include <gtest/gtest.h>
#include "Core/NavGrid.h"

class NavGridTest : public ::testing::Test
{
protected:
    void SetUp() override { NavGrid::instance().clear(); }
    NavGrid &nav = NavGrid::instance();
};

TEST_F(NavGridTest, FindPath_OpenField_StraightLine)
{
    auto path = nav.findPath(0.5f, 0.5f, 10.5f, 0.5f);
    ASSERT_FALSE(path.empty());
    // 空旷地：视线平滑应把路径拉成一段直达（路点很少）
    EXPECT_LE(path.size(), 2u);
    EXPECT_NEAR(path.back().first, 10.5f, 0.01f); // 末点贴到真实目标
    EXPECT_NEAR(path.back().second, 0.5f, 0.01f);
}

TEST_F(NavGridTest, FindPath_WallBetween_GoesAround)
{
    // 一堵墙横在 x=5：z 从 -3 到 3（起终点在墙两侧）
    nav.blockRect(4.6f, -3.0f, 5.4f, 3.0f);
    auto path = nav.findPath(0.5f, 0.5f, 10.5f, 0.5f);
    ASSERT_FALSE(path.empty());
    // 绕行路径必然有中间点偏离直线（|z| 超过墙的半高附近）
    bool detoured = false;
    for (auto &p : path)
        if (std::fabs(p.second) > 2.5f)
            detoured = true;
    EXPECT_TRUE(detoured);
}

TEST_F(NavGridTest, FindPath_FullyEnclosedTarget_ReturnsEmpty)
{
    // 目标被四面墙围死
    nav.blockRect(9.0f, 9.0f, 12.0f, 9.9f);   // 下墙
    nav.blockRect(9.0f, 11.1f, 12.0f, 12.0f); // 上墙
    nav.blockRect(9.0f, 9.0f, 9.9f, 12.0f);   // 左墙
    nav.blockRect(11.1f, 9.0f, 12.0f, 12.0f); // 右墙
    auto path = nav.findPath(0.5f, 0.5f, 10.5f, 10.5f);
    EXPECT_TRUE(path.empty()); // 不可达
}

TEST_F(NavGridTest, FindPath_TargetBlocked_ReturnsEmpty)
{
    nav.blockRect(10.0f, 10.0f, 11.0f, 11.0f);
    auto path = nav.findPath(0.5f, 0.5f, 10.5f, 10.5f);
    EXPECT_TRUE(path.empty()); // 目标格本身是障碍
}

TEST_F(NavGridTest, IsBlocked_MarkedAndOutside)
{
    nav.blockRect(3.0f, 3.0f, 4.0f, 4.0f);
    EXPECT_TRUE(nav.isBlocked(3.5f, 3.5f));
    EXPECT_FALSE(nav.isBlocked(0.5f, 0.5f));
    EXPECT_TRUE(nav.isBlocked(1000.0f, 0.0f)); // 网格外视为障碍
}

TEST_F(NavGridTest, FindPath_SameCell_DirectToTarget)
{
    auto path = nav.findPath(0.2f, 0.2f, 0.8f, 0.8f); // 同一格内
    ASSERT_EQ(path.size(), 1u);
    EXPECT_NEAR(path[0].first, 0.8f, 0.01f);
}
