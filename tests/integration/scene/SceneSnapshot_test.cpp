// ============================================================
// SceneSnapshot 集成测试 — Play/Stop 场景隔离（真实 flecs world）
//
// 覆盖：
//   capture → restore 还原实体组件数据
//   运行时新建实体被清除
//   父子关系重建
//   快照用完即废（hasSnapshot 状态）
//   未 capture 时 restore 安全无操作
//
// 注意：SceneSnapshot 依赖 SceneSerializer + flecs 反射，
//   需要 Engine 注册好全部组件（反射信息完整）。
// ============================================================
#include <gtest/gtest.h>
#include "Core/Engine.h"
#include "Core/SceneSnapshot.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include <cstring>

namespace
{
    template <typename T>
    const T *getComp(flecs::world &w, flecs::entity_t id)
    {
        return static_cast<const T *>(
            ecs_get_id(w.c_ptr(), id, w.component<T>().raw_id()));
    }

    /// 按 Tag 名查找实体（返回 entity_t，找不到返回 0）
    flecs::entity_t findByName(flecs::world &w, const char *name)
    {
        flecs::entity_t found = 0;
        w.each([&](flecs::entity e, const TagComponent &tag)
               {
            if (std::strcmp(tag.name, name) == 0)
                found = e.raw_id(); });
        return found;
    }

    /// 统计世界中实体数量
    int countEntities(flecs::world &w)
    {
        int count = 0;
        w.each([&](flecs::entity) { ++count; });
        return count;
    }
} // namespace

class SceneSnapshotTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = new Engine();
    }
    void TearDown() override
    {
        delete m_engine;
    }

    /// 创建带 Tag + Transform 的实体
    flecs::entity makeNamed(const char *name, glm::vec3 pos = {0, 0, 0})
    {
        auto e = m_engine->world().entity();
        TagComponent tc{};
        strncpy(tc.name, name, sizeof(tc.name) - 1);
        e.set<TagComponent>(tc);
        TransformComponent tf{};
        tf.position = pos;
        e.set<TransformComponent>(tf);
        return e;
    }

    Engine *m_engine = nullptr;
    SceneSnapshot m_snapshot;
};

// ---- 基本 capture/restore ----

TEST_F(SceneSnapshotTest, CaptureRestore_PreservesTransform)
{
    makeNamed("cube", {1.0f, 2.0f, 3.0f});
    m_snapshot.capture(m_engine->world());

    // 模拟运行时修改（用 ecs_get_mut_id 获取可写指针）
    auto id = findByName(m_engine->world(), "cube");
    ASSERT_NE(id, 0u);
    auto *tfMut = static_cast<TransformComponent *>(
        ecs_get_mut_id(m_engine->world().c_ptr(), id,
                       m_engine->world().component<TransformComponent>().raw_id()));
    ASSERT_NE(tfMut, nullptr);
    tfMut->position = {99.0f, 99.0f, 99.0f};

    // 恢复
    m_snapshot.restore(m_engine->world());

    // 验证位置还原
    id = findByName(m_engine->world(), "cube");
    ASSERT_NE(id, 0u);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    ASSERT_NE(tf, nullptr);
    EXPECT_FLOAT_EQ(tf->position.x, 1.0f);
    EXPECT_FLOAT_EQ(tf->position.y, 2.0f);
    EXPECT_FLOAT_EQ(tf->position.z, 3.0f);
}

TEST_F(SceneSnapshotTest, CaptureRestore_PreservesMultipleEntities)
{
    makeNamed("a", {1, 0, 0});
    makeNamed("b", {2, 0, 0});
    makeNamed("c", {3, 0, 0});

    m_snapshot.capture(m_engine->world());
    m_snapshot.restore(m_engine->world());

    EXPECT_NE(findByName(m_engine->world(), "a"), 0u);
    EXPECT_NE(findByName(m_engine->world(), "b"), 0u);
    EXPECT_NE(findByName(m_engine->world(), "c"), 0u);
}

// ---- 运行时新建实体被清除 ----

TEST_F(SceneSnapshotTest, Restore_RemovesRuntimeCreatedEntities)
{
    makeNamed("original", {0, 0, 0});
    m_snapshot.capture(m_engine->world());

    // 运行时新建实体（如子弹/特效）
    makeNamed("bullet", {5, 5, 5});
    int countBefore = countEntities(m_engine->world());
    EXPECT_GT(countBefore, 1);

    // 恢复 → 子弹应被清除
    m_snapshot.restore(m_engine->world());
    EXPECT_EQ(findByName(m_engine->world(), "bullet"), 0u);
    EXPECT_NE(findByName(m_engine->world(), "original"), 0u);
}

// ---- 快照状态 ----

TEST_F(SceneSnapshotTest, Initial_NoSnapshot)
{
    EXPECT_FALSE(m_snapshot.hasSnapshot());
}

TEST_F(SceneSnapshotTest, Capture_HasSnapshot)
{
    makeNamed("x", {0, 0, 0});
    m_snapshot.capture(m_engine->world());
    EXPECT_TRUE(m_snapshot.hasSnapshot());
}

TEST_F(SceneSnapshotTest, Restore_InvalidatesSnapshot)
{
    makeNamed("x", {0, 0, 0});
    m_snapshot.capture(m_engine->world());
    m_snapshot.restore(m_engine->world());
    EXPECT_FALSE(m_snapshot.hasSnapshot()); // 用完即废
}

TEST_F(SceneSnapshotTest, Restore_WithoutCapture_NoOp)
{
    makeNamed("safe", {1, 2, 3});
    // 未 capture 直接 restore → 不应崩溃/不应改变世界
    m_snapshot.restore(m_engine->world());
    auto id = findByName(m_engine->world(), "safe");
    ASSERT_NE(id, 0u);
    auto *tf = getComp<TransformComponent>(m_engine->world(), id);
    EXPECT_FLOAT_EQ(tf->position.x, 1.0f);
}

// ---- clear ----

TEST_F(SceneSnapshotTest, Clear_InvalidatesSnapshot)
{
    makeNamed("x", {0, 0, 0});
    m_snapshot.capture(m_engine->world());
    EXPECT_TRUE(m_snapshot.hasSnapshot());
    m_snapshot.clear();
    EXPECT_FALSE(m_snapshot.hasSnapshot());
}

// ---- 父子关系 ----

TEST_F(SceneSnapshotTest, CaptureRestore_RebuildsParentChild)
{
    // 创建父实体
    auto parent = makeNamed("parent", {0, 0, 0});
    // 创建子实体并设置父子关系
    auto child = makeNamed("child", {1, 0, 0});
    child.child_of(parent);

    m_snapshot.capture(m_engine->world());
    m_snapshot.restore(m_engine->world());

    // 验证子实体的父关系被重建
    auto childId = findByName(m_engine->world(), "child");
    ASSERT_NE(childId, 0u);
    flecs::entity childEnt = m_engine->world().entity(childId);
    flecs::entity restoredParent = childEnt.parent();
    EXPECT_TRUE(restoredParent && restoredParent.is_alive());
}

TEST_F(SceneSnapshotTest, CaptureRestore_ChildPositionPreserved)
{
    auto parent = makeNamed("parent", {10, 0, 0});
    auto child = makeNamed("child", {1, 2, 3});
    child.child_of(parent);

    m_snapshot.capture(m_engine->world());
    m_snapshot.restore(m_engine->world());

    auto childId = findByName(m_engine->world(), "child");
    ASSERT_NE(childId, 0u);
    auto *tf = getComp<TransformComponent>(m_engine->world(), childId);
    ASSERT_NE(tf, nullptr);
    // 局部坐标应保持
    EXPECT_FLOAT_EQ(tf->position.x, 1.0f);
    EXPECT_FLOAT_EQ(tf->position.y, 2.0f);
}

// ---- 材质组件保持 ----

TEST_F(SceneSnapshotTest, CaptureRestore_PreservesMaterial)
{
    auto e = makeNamed("shiny", {0, 0, 0});
    MaterialComponent mat{};
    mat.albedo = {1.0f, 0.0f, 0.0f}; // 红色
    mat.metallic = 0.8f;
    e.set<MaterialComponent>(mat);

    m_snapshot.capture(m_engine->world());
    m_snapshot.restore(m_engine->world());

    auto id = findByName(m_engine->world(), "shiny");
    ASSERT_NE(id, 0u);
    auto *m = getComp<MaterialComponent>(m_engine->world(), id);
    ASSERT_NE(m, nullptr);
    EXPECT_FLOAT_EQ(m->albedo.r, 1.0f);
    EXPECT_FLOAT_EQ(m->metallic, 0.8f);
}
