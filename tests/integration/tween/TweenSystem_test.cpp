// ============================================================
// TweenSystem 集成测试 — 属性补间（真实 flecs world，无 GUI）
//
// 覆盖：
//   补间启动/插值/完成
//   同实体同属性新补间顶掉旧的
//   实体销毁后补间自动移除
//   clearAll 清空
//   duration 边界（极短/零）
//
// 注意：physicsSetPosition 在无物理世界时安全返回 false，不影响测试。
//   完成信号走 g_signalQueue，测试中直接检查队列。
// ============================================================
#include <gtest/gtest.h>
#include "Core/Engine.h"
#include "Systems/TweenSystem.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Core/SignalQueue.h" // g_signalQueue（Core 层，不拉 Lua 依赖）
#include <cstring>

namespace
{
    template <typename T>
    const T *getComp(flecs::world &w, flecs::entity_t id)
    {
        return static_cast<const T *>(
            ecs_get_id(w.c_ptr(), id, w.component<T>().raw_id()));
    }
} // namespace

class TweenSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = new Engine();
        m_tween = new TweenSystem();
        TweenSystem::clearAll();
        // 清空信号队列（其他测试可能残留）
        while (!g_signalQueue.empty())
            g_signalQueue.pop();
    }
    void TearDown() override
    {
        TweenSystem::clearAll();
        delete m_tween;
        delete m_engine;
    }

    /// 创建带 Transform 的实体，返回 entity_t
    flecs::entity_t makeEntity(const char *name, glm::vec3 pos = {0, 0, 0})
    {
        auto e = m_engine->world().entity();
        TagComponent tc{};
        strncpy(tc.name, name, sizeof(tc.name) - 1);
        e.set<TagComponent>(tc);
        TransformComponent tf{};
        tf.position = pos;
        e.set<TransformComponent>(tf);
        return e.raw_id();
    }

    /// 步进 N 帧
    void stepFrames(int n, float dt = 1.0f / 60.0f)
    {
        for (int i = 0; i < n; ++i)
            m_tween->update(m_engine->world(), dt);
    }

    Engine *m_engine = nullptr;
    TweenSystem *m_tween = nullptr;
};

// ---- 基本补间 ----

TEST_F(TweenSystemTest, PositionTween_MovesEntity)
{
    auto id = makeEntity("box", {0, 0, 0});
    // 从当前位置(0,0,0)补间到(10,0,0)，1秒，线性
    tweenStart(id, "position", 10, 0, 0, 1.0f, 0, "");

    // 半程（30帧×1/60=0.5s）
    stepFrames(30);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->position.x, 5.0f, 0.5f); // 约一半（浮点帧精度）
}

TEST_F(TweenSystemTest, PositionTween_CompletesAtTarget)
{
    auto id = makeEntity("box", {0, 0, 0});
    tweenStart(id, "position", 10, 5, 3, 0.5f, 0, "");

    // 超过 duration（0.5s = 30帧@60fps，给 40 帧余量）
    stepFrames(40);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->position.x, 10.0f, 0.01f);
    EXPECT_NEAR(tf->position.y, 5.0f, 0.01f);
    EXPECT_NEAR(tf->position.z, 3.0f, 0.01f);
}

TEST_F(TweenSystemTest, RotationTween_ChangesRotation)
{
    auto id = makeEntity("spinner", {0, 0, 0});
    tweenStart(id, "rotation", 0, 180, 0, 1.0f, 0, "");

    stepFrames(60); // 1 秒
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->rotation.y, 180.0f, 1.0f);
}

TEST_F(TweenSystemTest, ScaleTween_ChangesScale)
{
    auto id = makeEntity("grower", {0, 0, 0});
    tweenStart(id, "scale", 2, 2, 2, 0.5f, 0, "");

    stepFrames(35); // 略超 0.5s
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->scale.x, 2.0f, 0.05f);
}

// ---- 同实体同属性替换 ----

TEST_F(TweenSystemTest, SameEntitySameTarget_NewerReplacesOlder)
{
    auto id = makeEntity("box", {0, 0, 0});
    tweenStart(id, "position", 100, 0, 0, 2.0f, 0, "");
    tweenStart(id, "position", 5, 0, 0, 0.5f, 0, ""); // 顶掉上一个

    stepFrames(35); // 超过第二个 tween 的 0.5s
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    // 应该到 5 而不是 100
    EXPECT_NEAR(tf->position.x, 5.0f, 0.1f);
}

TEST_F(TweenSystemTest, SameEntityDiffTarget_BothRun)
{
    auto id = makeEntity("box", {0, 0, 0});
    tweenStart(id, "position", 10, 0, 0, 0.5f, 0, "");
    tweenStart(id, "scale", 3, 3, 3, 0.5f, 0, "");

    stepFrames(35);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    // 两个补间都完成
    EXPECT_NEAR(tf->position.x, 10.0f, 0.1f);
    EXPECT_NEAR(tf->scale.x, 3.0f, 0.1f);
}

// ---- 实体销毁 ----

TEST_F(TweenSystemTest, EntityDestroyed_TweenRemovedGracefully)
{
    auto id = makeEntity("doomed", {0, 0, 0});
    tweenStart(id, "position", 10, 0, 0, 1.0f, 0, "");

    // 销毁实体
    m_engine->world().entity(id).destruct();

    // 不应崩溃
    stepFrames(10);
    // 验证世界中没有该实体
    EXPECT_FALSE(m_engine->world().entity(id).is_alive());
}

// ---- 完成信号 ----

TEST_F(TweenSystemTest, DoneSignal_PushedToQueue)
{
    auto id = makeEntity("signaler", {0, 0, 0});
    tweenStart(id, "position", 5, 0, 0, 0.1f, 0, "move_done");

    stepFrames(10); // 超过 0.1s
    // 信号应该被推入队列
    ASSERT_FALSE(g_signalQueue.empty());
    auto sig = g_signalQueue.front();
    EXPECT_EQ(sig.emitterId, id);
    EXPECT_EQ(sig.signalName, "move_done");
}

TEST_F(TweenSystemTest, NoDoneSignal_QueueStaysEmpty)
{
    auto id = makeEntity("quiet", {0, 0, 0});
    tweenStart(id, "position", 5, 0, 0, 0.1f, 0, ""); // 空信号名

    stepFrames(10);
    EXPECT_TRUE(g_signalQueue.empty());
}

// ---- clearAll ----

TEST_F(TweenSystemTest, ClearAll_StopsAllTweens)
{
    auto id = makeEntity("box", {0, 0, 0});
    tweenStart(id, "position", 100, 0, 0, 5.0f, 0, "");
    stepFrames(5); // 开始移动了一点

    TweenSystem::clearAll();
    stepFrames(60); // 即使再走很多帧，补间已清空

    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    // 位置不应该到 100（补间被清除了）
    EXPECT_LT(tf->position.x, 50.0f);
}

// ---- duration 边界 ----

TEST_F(TweenSystemTest, VeryShortDuration_CompletesInOneFrame)
{
    auto id = makeEntity("fast", {0, 0, 0});
    tweenStart(id, "position", 10, 0, 0, 0.001f, 0, ""); // 极短

    stepFrames(1);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->position.x, 10.0f, 0.01f);
}

TEST_F(TweenSystemTest, ZeroDuration_ClampedToMinimum)
{
    auto id = makeEntity("instant", {0, 0, 0});
    tweenStart(id, "position", 10, 0, 0, 0.0f, 0, ""); // 0 → 钳到 0.001

    stepFrames(2);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_NEAR(tf->position.x, 10.0f, 0.01f);
}

// ---- 缓动效果 ----

TEST_F(TweenSystemTest, EaseOutQuad_MidPointAheadOfLinear)
{
    auto id = makeEntity("eased", {0, 0, 0});
    // EaseOutQuad = 枚举值 2
    tweenStart(id, "position", 10, 0, 0, 1.0f, 2, "");

    stepFrames(30); // 半程
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    // EaseOut 前半段快于线性 → 半程时 position > 5
    EXPECT_GT(tf->position.x, 5.0f);
}
