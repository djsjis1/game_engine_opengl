// ============================================================
// ParticleSystem 集成测试（纯 CPU 模拟，无 GL——数据与呈现分离的红利）
// 覆盖：发射速率 / 池上限 / 寿命回收 / 重力积分 / 停发自然消散
// ============================================================
#include <gtest/gtest.h>
#include "Core/Engine.h"
#include "Systems/ParticleSystem.h"
#include "Components/TransformComponent.h"
#include "Components/ParticleComponent.h"
#include "Components/TagComponent.h"

namespace
{
    template <typename T>
    const T *getComp(flecs::world &w, flecs::entity e)
    {
        return static_cast<const T *>(
            ecs_get_id(w.c_ptr(), e.raw_id(), w.component<T>().raw_id()));
    }
} // namespace

class ParticleSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = new Engine();
        m_sys = new ParticleSystem();
    }
    void TearDown() override
    {
        delete m_sys;
        delete m_engine;
    }

    flecs::entity makeEmitter(float rate, float lifetime, int maxP = 1000)
    {
        auto e = m_engine->world().entity();
        e.set<TagComponent>({"Emitter"});
        e.set<TransformComponent>({});
        ParticleComponent pc{};
        pc.rate = rate;
        pc.lifetime = lifetime;
        pc.maxParticles = maxP;
        e.set<ParticleComponent>(pc);
        return e;
    }

    void stepFrames(int n, float dt = 1.0f / 60.0f)
    {
        for (int i = 0; i < n; ++i)
            m_sys->update(m_engine->world(), dt);
    }

    Engine *m_engine = nullptr;
    ParticleSystem *m_sys = nullptr;
};

TEST_F(ParticleSystemTest, Update_OneSecondAt60fps_EmitsRateParticles)
{
    auto e = makeEmitter(30.0f, 10.0f); // 30/秒，寿命足够长不回收
    stepFrames(60);                     // 1 秒
    const auto *pc = getComp<ParticleComponent>(m_engine->world(), e);
    // 累积器语义：1 秒 ≈ 30 个（±1 的浮点余量）
    EXPECT_GE((int)pc->pool.size(), 29);
    EXPECT_LE((int)pc->pool.size(), 31);
}

TEST_F(ParticleSystemTest, Update_PoolCap_NeverExceedsMaxParticles)
{
    auto e = makeEmitter(10000.0f, 10.0f, 50); // 疯狂发射，上限 50
    stepFrames(30);
    const auto *pc = getComp<ParticleComponent>(m_engine->world(), e);
    EXPECT_LE((int)pc->pool.size(), 50);
}

TEST_F(ParticleSystemTest, Update_ExpiredParticles_AreRecycled)
{
    auto e = makeEmitter(60.0f, 0.2f); // 寿命仅 0.2s
    stepFrames(60);                    // 跑 1 秒：只有最近 0.2s 出生的活着
    const auto *pc = getComp<ParticleComponent>(m_engine->world(), e);
    // 稳态存活 ≈ rate * lifetime = 12（给宽容差）
    EXPECT_GT((int)pc->pool.size(), 5);
    EXPECT_LT((int)pc->pool.size(), 20);
}

TEST_F(ParticleSystemTest, Update_Gravity_AcceleratesParticlesDownward)
{
    auto e = makeEmitter(60.0f, 10.0f);
    {
        // 定制：无初速 + 强下重力 → 粒子必然下落
        auto *pc = (ParticleComponent *)ecs_get_mut_id(
            m_engine->world().c_ptr(), e.raw_id(),
            m_engine->world().component<ParticleComponent>().raw_id());
        pc->speed = 0.0f;
        pc->gravity = {0, -10.0f, 0};
    }
    stepFrames(60); // 1 秒
    const auto *pc = getComp<ParticleComponent>(m_engine->world(), e);
    ASSERT_FALSE(pc->pool.empty());
    // 最早出生的粒子（age 最大）应已明显下落
    float minY = 0.0f;
    for (const auto &p : pc->pool)
        minY = std::min(minY, p.pos.y);
    EXPECT_LT(minY, -0.5f);
}

TEST_F(ParticleSystemTest, EmittingOff_ExistingParticlesFadeNaturally)
{
    auto e = makeEmitter(60.0f, 0.5f);
    stepFrames(30); // 攒一些粒子
    {
        auto *pc = (ParticleComponent *)ecs_get_mut_id(
            m_engine->world().c_ptr(), e.raw_id(),
            m_engine->world().component<ParticleComponent>().raw_id());
        ASSERT_FALSE(pc->pool.empty());
        pc->emitting = false; // 停发（particles_emit(id,false) 的效果）
    }
    stepFrames(60); // 1 秒后：不再新增，旧的全部超寿命回收
    const auto *pc = getComp<ParticleComponent>(m_engine->world(), e);
    EXPECT_TRUE(pc->pool.empty());
}
