// ============================================================
// PhysicsSystem 集成测试（真实 Jolt + flecs world，无 GUI）
//
// 重点：把实战踩过的坑固化为回归测试——
//   坑17 刚体睡眠后 set/impulse 失效（ActivateBody）
//   坑19 落地射线打中自己（ignoreEntity 过滤器）
//   坑21 set_position 被物理回写拉回（physicsSetPosition 双通道）
//   坑22 默认密度千斤方块吞掉冲量（mass 传递给 Jolt）
// 详见 docs/troubleshooting.md
// ============================================================
#include <gtest/gtest.h>
#include <cmath>
#include "Core/Engine.h"
#include "Systems/PhysicsSystem.h"
#include "Components/TransformComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/SphereColliderComponent.h"
#include "Components/CapsuleColliderComponent.h"
#include "Components/TagComponent.h"

namespace
{
    constexpr float kStep = 1.0f / 60.0f;

    // 读组件（MSVC 下 e.get<T>() 有模板 bug，用 C API——坑2 的标准绕法）
    template <typename T>
    const T *getComp(flecs::world &w, flecs::entity e)
    {
        return static_cast<const T *>(
            ecs_get_id(w.c_ptr(), e.raw_id(), w.component<T>().raw_id()));
    }
} // namespace

class PhysicsSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = new Engine();
        m_phys = new PhysicsSystem();
        m_phys->init();
        // 直接驱动 System（不走 Engine 管线，隔离测试物理本身）
    }
    void TearDown() override
    {
        m_phys->shutdown();
        delete m_phys;
        delete m_engine;
    }

    // 建一个带刚体+碰撞体的立方体实体
    flecs::entity makeCube(glm::vec3 pos, int motionType, float mass = 1.0f)
    {
        auto e = m_engine->world().entity();
        e.set<TagComponent>({"cube"});
        TransformComponent tf;
        tf.position = pos;
        e.set<TransformComponent>(tf);
        RigidbodyComponent rb;
        rb.motionType = motionType;
        rb.mass = mass;
        e.set<RigidbodyComponent>(rb);
        e.set<BoxColliderComponent>({});
        return e;
    }

    void stepFrames(int n)
    {
        for (int i = 0; i < n; ++i)
            m_phys->update(m_engine->world(), kStep);
    }

    Engine *m_engine = nullptr;
    PhysicsSystem *m_phys = nullptr;
};

// ---- 刚体创建 ----

TEST_F(PhysicsSystemTest, Update_DynamicEntity_AssignsBodyId)
{
    auto e = makeCube({0, 5, 0}, 2);
    stepFrames(1); // 首帧惰性建 body
    const auto *rb = getComp<RigidbodyComponent>(m_engine->world(), e);
    ASSERT_NE(rb, nullptr);
    EXPECT_NE(rb->bodyId, 0u);
}

// ---- 重力 ----

TEST_F(PhysicsSystemTest, Update_DynamicBody_FallsUnderGravity)
{
    auto e = makeCube({0, 5, 0}, 2);
    stepFrames(30); // 半秒
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    ASSERT_NE(tf, nullptr);
    EXPECT_LT(tf->position.y, 4.0f); // 明显下落
}

TEST_F(PhysicsSystemTest, Update_StaticBody_DoesNotFall)
{
    auto e = makeCube({0, 5, 0}, 0); // 静态
    stepFrames(30);
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_FLOAT_EQ(tf->position.y, 5.0f); // 纹丝不动
}

// ---- 速度接口 ----

TEST_F(PhysicsSystemTest, SetGetVelocity_RoundTrip)
{
    auto e = makeCube({0, 5, 0}, 2);
    stepFrames(1);
    ASSERT_TRUE(physicsSetVelocity(e.raw_id(), 3.0f, 0.0f, -2.0f));
    float vx = 0, vy = 0, vz = 0;
    ASSERT_TRUE(physicsGetVelocity(e.raw_id(), vx, vy, vz));
    EXPECT_NEAR(vx, 3.0f, 0.01f);
    EXPECT_NEAR(vz, -2.0f, 0.01f);
}

TEST_F(PhysicsSystemTest, SetVelocity_NoBody_ReturnsFalse)
{
    EXPECT_FALSE(physicsSetVelocity(999999, 1, 0, 0)); // 不存在的实体
}

// ---- 角色控制器（CharacterBody3D 同款两大件）----

TEST_F(PhysicsSystemTest, MoveSlide_PreservesVerticalVelocity)
{
    auto e = makeCube({0, 50, 0}, 2);
    stepFrames(1);
    physicsSetVelocity(e.raw_id(), 0, -7.5f, 0); // 模拟下落中
    ASSERT_TRUE(physicsMoveSlide(e.raw_id(), 3.0f, -2.0f));
    float vx = 0, vy = 0, vz = 0;
    physicsGetVelocity(e.raw_id(), vx, vy, vz);
    EXPECT_NEAR(vx, 3.0f, 0.01f);
    EXPECT_NEAR(vy, -7.5f, 0.01f); // ★ 竖直速度必须原样保留
    EXPECT_NEAR(vz, -2.0f, 0.01f);
}

TEST_F(PhysicsSystemTest, IsOnFloor_GroundedTrue_AirborneFalse)
{
    auto ground = makeCube({0, -0.5f, 0}, 0);  // 地面（顶面 y=0）
    auto standing = makeCube({0, 0.5f, 0}, 2); // 站地上
    auto airborne = makeCube({5, 30, 5}, 2);   // 高空坠落中
    stepFrames(3);
    EXPECT_TRUE(physicsIsOnFloor(standing.raw_id()));
    EXPECT_FALSE(physicsIsOnFloor(airborne.raw_id()));
}

// ---- 坑22 回归：冲量 ÷ 质量 = 速度变化（mass 必须传给 Jolt）----

TEST_F(PhysicsSystemTest, ApplyImpulse_UnitMass_DeltaVEqualsImpulse)
{
    auto e = makeCube({0, 50, 0}, 2, 1.0f); // mass=1，放高处避免落地摩擦
    stepFrames(1);
    physicsSetVelocity(e.raw_id(), 0, 0, 0);
    ASSERT_TRUE(physicsApplyImpulse(e.raw_id(), 0.0f, 6.0f, 0.0f));
    float vx = 0, vy = 0, vz = 0;
    physicsGetVelocity(e.raw_id(), vx, vy, vz);
    // mass=1 → Δv = 6/1 = 6。当年 Jolt 默认密度 1000kg 时这里只有 0.006（坑22）
    EXPECT_NEAR(vy, 6.0f, 0.1f);
}

TEST_F(PhysicsSystemTest, ApplyImpulse_HalfMass_DeltaVDoubles)
{
    auto e = makeCube({0, 50, 0}, 2, 0.5f); // mass=0.5
    stepFrames(1);
    physicsSetVelocity(e.raw_id(), 0, 0, 0);
    physicsApplyImpulse(e.raw_id(), 0.0f, 6.0f, 0.0f);
    float vx = 0, vy = 0, vz = 0;
    physicsGetVelocity(e.raw_id(), vx, vy, vz);
    EXPECT_NEAR(vy, 12.0f, 0.2f); // Δv = 6/0.5 = 12
}

// ---- 坑19 回归：射线忽略发射者（从自身中心发不打中自己）----

TEST_F(PhysicsSystemTest, Raycast_FromInsideSelf_IgnoresSelfHitsGround)
{
    auto ground = makeCube({0, -0.5f, 0}, 0); // 静态地面（顶面 y=0）
    auto player = makeCube({0, 0.5f, 0}, 2);  // 站地面上（中心 y=0.5）
    stepFrames(2);

    flecs::entity_t hitE = 0;
    float hx = 0, hy = 0, hz = 0, hd = 0;
    // 从玩家中心向下 0.65：不忽略自己会命中自己（fraction 0）
    bool hit = physicsRaycast(0, 0.5f, 0, 0, -1, 0, 0.65f,
                              hitE, hx, hy, hz, hd, player.raw_id());
    ASSERT_TRUE(hit);
    EXPECT_EQ(hitE, ground.raw_id()); // 打中的是地面，不是自己
}

TEST_F(PhysicsSystemTest, Raycast_NothingBelow_ReturnsFalse)
{
    auto lone = makeCube({0, 100, 0}, 2); // 悬空，下方无物
    stepFrames(1);
    flecs::entity_t hitE = 0;
    float hx = 0, hy = 0, hz = 0, hd = 0;
    EXPECT_FALSE(physicsRaycast(0, 100, 0, 0, -1, 0, 2.0f,
                                hitE, hx, hy, hz, hd, lone.raw_id()));
}

TEST_F(PhysicsSystemTest, SphereCollider_RaycastUsesCurvedSurface)
{
    auto e = makeCube({0, 0, 0}, 0);
    e.remove<BoxColliderComponent>();
    e.set<SphereColliderComponent>({0.5f, false});
    stepFrames(1);

    flecs::entity_t hitE = 0;
    float hx = 0, hy = 0, hz = 0, hitDist = 0;
    ASSERT_TRUE(physicsRaycast(-2.0f, 0.49f, 0.0f, 1, 0, 0, 4.0f,
                               hitE, hx, hy, hz, hitDist));
    EXPECT_EQ(hitE, e.raw_id());
    // A unit box would hit at 1.5; the sphere's narrow chord is near 1.9.
    EXPECT_NEAR(hitDist, 2.0f - std::sqrt(0.25f - 0.49f * 0.49f), 0.02f);
}

TEST_F(PhysicsSystemTest, CapsuleCollider_RaycastUsesRoundedCap)
{
    auto e = makeCube({0, 0, 0}, 0);
    e.remove<BoxColliderComponent>();
    e.set<CapsuleColliderComponent>({0.5f, 2.0f, false});
    stepFrames(1);

    flecs::entity_t hitE = 0;
    float hx = 0, hy = 0, hz = 0, hitDist = 0;
    ASSERT_TRUE(physicsRaycast(-2.0f, 0.9f, 0.0f, 1, 0, 0, 4.0f,
                               hitE, hx, hy, hz, hitDist));
    EXPECT_EQ(hitE, e.raw_id());
    // At y=.9, the top hemisphere is centered at y=.5 and has x radius .3.
    EXPECT_NEAR(hitDist, 1.7f, 0.03f);
}

// ---- 坑21 回归：传送必须写物理体（且插值快照同步，不产生拖影滑移）----

TEST_F(PhysicsSystemTest, SetPosition_Teleport_TransformFollowsImmediately)
{
    auto e = makeCube({0, 5, 0}, 2);
    stepFrames(5);
    ASSERT_TRUE(physicsSetPosition(e.raw_id(), 10.0f, 20.0f, 30.0f));
    stepFrames(1); // 一帧后 Transform 应已在新位置（快照已重置，无滑移）
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_NEAR(tf->position.x, 10.0f, 0.5f);
    EXPECT_NEAR(tf->position.y, 20.0f, 0.5f); // 允许一步重力误差
    EXPECT_NEAR(tf->position.z, 30.0f, 0.5f);
}

// ---- resetBodies：退出 Play 清场 ----

TEST_F(PhysicsSystemTest, ResetBodies_BodiesRecreatedNextUpdate)
{
    auto e = makeCube({0, 5, 0}, 2);
    stepFrames(1);
    uint32_t firstId = getComp<RigidbodyComponent>(m_engine->world(), e)->bodyId;
    ASSERT_NE(firstId, 0u);

    m_phys->resetBodies();
    stepFrames(1); // 下帧从当前 Transform 重建
    uint32_t secondId = getComp<RigidbodyComponent>(m_engine->world(), e)->bodyId;
    EXPECT_NE(secondId, 0u); // 重建成功（ID 可能不同，但必须有效）
}
