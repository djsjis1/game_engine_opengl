// ============================================================
// AnimationTrack 单元测试 — 动画轨道采样数学 + 事件跨越检测
//
// 覆盖：
//   AnimTrack::sample()：线性插值/边界钳制/缓动/空轨道
//   AnimTrack::setKey()：插入排序/同时刻覆盖
//   AnimClip::collectEvents()：正放跨越/回绕拆段/倒放/无事件
//
// 注意：本文件 include AnimationLibrary.h（含 flecs.h），
//   编译需链 flecs 库，放在 integration_tests 目标中。
// ============================================================
#include <gtest/gtest.h>
#include "Render/AnimationLibrary.h"
#include <cmath>

// ---- AnimTrack::sample ----

class AnimTrackTest : public ::testing::Test
{
protected:
    AnimTrack m_track;
};

TEST_F(AnimTrackTest, Sample_EmptyTrack_ReturnsZero)
{
    glm::vec3 v = m_track.sample(0.5f);
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST_F(AnimTrackTest, Sample_SingleKey_ReturnsConstant)
{
    m_track.setKey(0.0f, {1.0f, 2.0f, 3.0f});
    // 任何时刻都返回唯一帧的值
    glm::vec3 v = m_track.sample(0.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    v = m_track.sample(99.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
}

TEST_F(AnimTrackTest, Sample_BeforeFirstKey_ClampsToFirst)
{
    m_track.setKey(1.0f, {5.0f, 0.0f, 0.0f});
    m_track.setKey(2.0f, {10.0f, 0.0f, 0.0f});
    // time=0.5 早于首帧(1.0) → 钉在首帧值
    glm::vec3 v = m_track.sample(0.5f);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
}

TEST_F(AnimTrackTest, Sample_AfterLastKey_ClampsToLast)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {10.0f, 0.0f, 0.0f});
    // time=5.0 晚于末帧(1.0) → 钉在末帧值
    glm::vec3 v = m_track.sample(5.0f);
    EXPECT_FLOAT_EQ(v.x, 10.0f);
}

TEST_F(AnimTrackTest, Sample_MidPoint_LinearInterpolation)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(2.0f, {10.0f, 20.0f, 30.0f});
    // time=1.0 正好中点 → alpha=0.5
    glm::vec3 v = m_track.sample(1.0f);
    EXPECT_NEAR(v.x, 5.0f, 0.001f);
    EXPECT_NEAR(v.y, 10.0f, 0.001f);
    EXPECT_NEAR(v.z, 15.0f, 0.001f);
}

TEST_F(AnimTrackTest, Sample_QuarterPoint_CorrectLerp)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(4.0f, {100.0f, 0.0f, 0.0f});
    // time=1.0 → alpha = 1/4 = 0.25
    glm::vec3 v = m_track.sample(1.0f);
    EXPECT_NEAR(v.x, 25.0f, 0.01f);
}

TEST_F(AnimTrackTest, Sample_MultipleKeys_CorrectSegment)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {10.0f, 0.0f, 0.0f});
    m_track.setKey(2.0f, {50.0f, 0.0f, 0.0f});
    // time=0.5 → 在第一段 [0,1] 中，alpha=0.5 → 5.0
    glm::vec3 v = m_track.sample(0.5f);
    EXPECT_NEAR(v.x, 5.0f, 0.001f);
    // time=1.5 → 在第二段 [1,2] 中，alpha=0.5 → 10+(50-10)*0.5=30
    v = m_track.sample(1.5f);
    EXPECT_NEAR(v.x, 30.0f, 0.001f);
}

TEST_F(AnimTrackTest, Sample_ExactlyOnKey_ReturnsKeyValue)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {10.0f, 0.0f, 0.0f});
    m_track.setKey(2.0f, {20.0f, 0.0f, 0.0f});
    // 恰好在关键帧时刻
    glm::vec3 v = m_track.sample(1.0f);
    EXPECT_NEAR(v.x, 10.0f, 0.001f);
}

// ---- AnimTrack::setKey ----

TEST_F(AnimTrackTest, SetKey_OverwritesSameTime)
{
    m_track.setKey(1.0f, {5.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {99.0f, 0.0f, 0.0f}); // 同时刻覆盖
    ASSERT_EQ(m_track.keys.size(), 1u);
    EXPECT_FLOAT_EQ(m_track.keys[0].v.x, 99.0f);
}

TEST_F(AnimTrackTest, SetKey_NearSameTime_Overwrites)
{
    m_track.setKey(1.0f, {5.0f, 0.0f, 0.0f});
    m_track.setKey(1.005f, {99.0f, 0.0f, 0.0f}); // ±10ms 内视为同时刻
    ASSERT_EQ(m_track.keys.size(), 1u);
    EXPECT_FLOAT_EQ(m_track.keys[0].v.x, 99.0f);
}

TEST_F(AnimTrackTest, SetKey_MaintainsTimeOrder)
{
    m_track.setKey(2.0f, {2.0f, 0.0f, 0.0f});
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {1.0f, 0.0f, 0.0f});
    ASSERT_EQ(m_track.keys.size(), 3u);
    EXPECT_FLOAT_EQ(m_track.keys[0].t, 0.0f);
    EXPECT_FLOAT_EQ(m_track.keys[1].t, 1.0f);
    EXPECT_FLOAT_EQ(m_track.keys[2].t, 2.0f);
}

TEST_F(AnimTrackTest, RemoveKeyAt_ValidIndex_Removes)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.setKey(1.0f, {1.0f, 0.0f, 0.0f});
    m_track.removeKeyAt(0);
    ASSERT_EQ(m_track.keys.size(), 1u);
    EXPECT_FLOAT_EQ(m_track.keys[0].t, 1.0f);
}

TEST_F(AnimTrackTest, RemoveKeyAt_InvalidIndex_NoOp)
{
    m_track.setKey(0.0f, {0.0f, 0.0f, 0.0f});
    m_track.removeKeyAt(-1);
    m_track.removeKeyAt(99);
    EXPECT_EQ(m_track.keys.size(), 1u);
}

// ---- AnimClip::collectEvents ----

class AnimClipEventTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_clip.duration = 2.0f;
        m_clip.addEvent(0.5f, "hit");
        m_clip.addEvent(1.0f, "jump");
        m_clip.addEvent(1.8f, "land");
    }

    AnimClip m_clip;
};

TEST_F(AnimClipEventTest, CollectEvents_CrossesOne_Fires)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // 从 0.4 → 0.6，跨过 0.5
    m_clip.collectEvents(0.4f, 0.6f, false, fired);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->signal, "hit");
}

TEST_F(AnimClipEventTest, CollectEvents_NoCross_Empty)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // 从 0.1 → 0.3，没跨过任何事件
    m_clip.collectEvents(0.1f, 0.3f, false, fired);
    EXPECT_TRUE(fired.empty());
}

TEST_F(AnimClipEventTest, CollectEvents_CrossesMultiple_AllFired)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // 从 0.3 → 1.5，跨过 0.5 和 1.0
    m_clip.collectEvents(0.3f, 1.5f, false, fired);
    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0]->signal, "hit");
    EXPECT_EQ(fired[1]->signal, "jump");
}

TEST_F(AnimClipEventTest, CollectEvents_ExactlyAtEvent_Fires)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // cur 恰好等于事件时刻（半开区间 (prev, cur] 包含 cur 端）
    m_clip.collectEvents(0.3f, 0.5f, false, fired);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->signal, "hit");
}

TEST_F(AnimClipEventTest, CollectEvents_ExactlyAtPrev_NotFired)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // prev 恰好等于事件时刻（半开区间不含 prev 端——上一帧已触发过）
    m_clip.collectEvents(0.5f, 0.7f, false, fired);
    EXPECT_TRUE(fired.empty());
}

TEST_F(AnimClipEventTest, CollectEvents_Wrapped_TailAndHead)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // 回绕：prev=1.7 → cur=0.1（wrapped=true，duration=2.0）
    // 应检测 (1.7, 2.0] + [0, 0.1]
    m_clip.collectEvents(1.7f, 0.1f, true, fired);
    // 事件 1.8 在尾段 (1.7, 2.0] 内
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->signal, "land");
}

TEST_F(AnimClipEventTest, CollectEvents_Wrapped_HeadEvent)
{
    // 加一个靠近 0 的事件
    m_clip.addEvent(0.05f, "start");

    std::vector<const AnimClip::AnimEvent *> fired;
    // 回绕：prev=1.9 → cur=0.1
    m_clip.collectEvents(1.9f, 0.1f, true, fired);
    // 0.05 在头段 [0, 0.1] 内
    bool foundStart = false;
    for (auto *ev : fired)
        if (ev->signal == "start")
            foundStart = true;
    EXPECT_TRUE(foundStart);
}

TEST_F(AnimClipEventTest, CollectEvents_ReversePlay_Fires)
{
    std::vector<const AnimClip::AnimEvent *> fired;
    // 倒放：prev=1.2 → cur=0.4（cur < prev，非 wrapped）
    // 倒放区间 [cur, prev) = [0.4, 1.2)
    m_clip.collectEvents(1.2f, 0.4f, false, fired);
    // 事件 0.5 和 1.0 在 [0.4, 1.2) 内
    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0]->signal, "hit");
    EXPECT_EQ(fired[1]->signal, "jump");
}

TEST_F(AnimClipEventTest, CollectEvents_NoEvents_Empty)
{
    AnimClip emptyClip;
    emptyClip.duration = 1.0f;
    std::vector<const AnimClip::AnimEvent *> fired;
    emptyClip.collectEvents(0.0f, 0.5f, false, fired);
    EXPECT_TRUE(fired.empty());
}

// ---- AnimClip::addEvent ----

TEST_F(AnimClipEventTest, AddEvent_Duplicate_NotAdded)
{
    size_t before = m_clip.events.size();
    m_clip.addEvent(0.5f, "hit"); // 同时刻同信号 → 不重复添加
    EXPECT_EQ(m_clip.events.size(), before);
}

TEST_F(AnimClipEventTest, AddEvent_SameTimeDiffSignal_Added)
{
    size_t before = m_clip.events.size();
    m_clip.addEvent(0.5f, "other"); // 同时刻不同信号 → 添加
    EXPECT_EQ(m_clip.events.size(), before + 1);
}

TEST_F(AnimClipEventTest, RemoveEventAt_ValidIndex_Removes)
{
    size_t before = m_clip.events.size();
    m_clip.removeEventAt(0);
    EXPECT_EQ(m_clip.events.size(), before - 1);
}

TEST_F(AnimClipEventTest, RemoveEventAt_InvalidIndex_NoOp)
{
    size_t before = m_clip.events.size();
    m_clip.removeEventAt(-1);
    m_clip.removeEventAt(999);
    EXPECT_EQ(m_clip.events.size(), before);
}

// ---- AnimClip::track (get-or-create) ----

TEST_F(AnimClipEventTest, Track_GetExisting_ReturnsSame)
{
    AnimTrack &t1 = m_clip.track("TransformComponent", "position");
    t1.setKey(0.0f, {1.0f, 0.0f, 0.0f});
    AnimTrack &t2 = m_clip.track("TransformComponent", "position");
    EXPECT_EQ(t2.keys.size(), 1u); // 同一条轨道
}

TEST_F(AnimClipEventTest, Track_CreateNew_AddsToList)
{
    EXPECT_TRUE(m_clip.tracks.empty());
    m_clip.track("TransformComponent", "position");
    m_clip.track("MaterialComponent", "albedo");
    EXPECT_EQ(m_clip.tracks.size(), 2u);
}
