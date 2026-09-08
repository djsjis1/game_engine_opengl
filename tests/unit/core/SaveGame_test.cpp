// ============================================================
// SaveGame 单元测试 — 内存草稿 / 槽位落盘往返 / 坏档保护
// 存档写测试进程 exe 旁 saves/（相对 exe，不依赖特定机器路径）
// 隔离：单例，SetUp 清草稿；用高槽位号避免误碰真实存档
// ============================================================
#include <gtest/gtest.h>
#include "Core/SaveGame.h"
#include <QCoreApplication>
#include <fstream>

namespace
{
    constexpr int kTestSlot = 90; // 测试专用槽位（远离玩家常用 1~9）
    constexpr int kBadSlot = 91;  // 坏档测试槽位

    std::string slotFile(int slot)
    {
        return (QCoreApplication::applicationDirPath() +
                QString("/saves/save%1.json").arg(slot))
            .toStdString();
    }
} // namespace

class SaveGameTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SaveGame::instance().clear();
        std::remove(slotFile(kTestSlot).c_str()); // 清上次测试残留
        std::remove(slotFile(kBadSlot).c_str());
    }
    SaveGame &save = SaveGame::instance();
};

// ---- 内存草稿 ----

TEST_F(SaveGameTest, SetGet_NumberStringBool_RoundTrip)
{
    save.set("hp", 99.5);
    save.set("name", "hero");
    save.set("done", true);
    EXPECT_DOUBLE_EQ(save.get("hp").get<double>(), 99.5);
    EXPECT_EQ(save.get("name").get<std::string>(), "hero");
    EXPECT_TRUE(save.get("done").get<bool>());
}

TEST_F(SaveGameTest, Get_MissingKey_ReturnsNullJson)
{
    EXPECT_TRUE(save.get("nope").is_null());
}

TEST_F(SaveGameTest, Clear_RemovesAllDraftData)
{
    save.set("hp", 1);
    save.clear();
    EXPECT_TRUE(save.get("hp").is_null());
}

// ---- 落盘 / 读档往返 ----

TEST_F(SaveGameTest, WriteLoad_DraftSurvivesDiskRoundTrip)
{
    save.set("level", "scenes/level2.json");
    save.set("px", 3.5);
    ASSERT_TRUE(save.write(kTestSlot));

    save.clear(); // 模拟重启后内存为空
    ASSERT_TRUE(save.load(kTestSlot));
    EXPECT_EQ(save.get("level").get<std::string>(), "scenes/level2.json");
    EXPECT_DOUBLE_EQ(save.get("px").get<double>(), 3.5);
}

TEST_F(SaveGameTest, Exists_BeforeAndAfterWrite_ReflectsFile)
{
    EXPECT_FALSE(save.exists(kTestSlot));
    save.set("x", 1);
    save.write(kTestSlot);
    EXPECT_TRUE(save.exists(kTestSlot));
}

TEST_F(SaveGameTest, Load_MissingSlot_ReturnsFalse)
{
    EXPECT_FALSE(save.load(kTestSlot)); // SetUp 已删文件
}

// ---- 坏档保护（异常测试）----

TEST_F(SaveGameTest, Load_CorruptedFile_ReturnsFalseAndKeepsDraft)
{
    // 手工写一个损坏的存档文件
    {
        std::ofstream f(slotFile(kBadSlot));
        f << "{ not valid json !!!";
    }
    save.set("progress", 42); // 内存里有有效进度
    EXPECT_FALSE(save.load(kBadSlot));
    // 关键：坏档不得覆盖内存草稿（别用损坏数据毁掉玩家进度）
    EXPECT_DOUBLE_EQ(save.get("progress").get<double>(), 42.0);
}
