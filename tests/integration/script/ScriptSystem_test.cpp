// ============================================================
// ScriptSystem 集成测试（真实 Lua VM + flecs world，无 GUI）
//
// 重点回归：
//   坑26 脚本环境跨 Play 会话残留（resetAllScripts 后 ready 必须重跑）
//   沙盒隔离（两个实体的 local 变量互不可见）
// 断言手法：脚本把结果写进自己的 Transform.position（探针模式，
//   与当年排障的"位置编码"诊断同思路——C++ 侧读 Transform 验证）
// ============================================================
#include <gtest/gtest.h>
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Systems/ScriptSystem.h"
#include "Components/TransformComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/SignalComponent.h"
#include "Components/TagComponent.h"
#include <cstring>

namespace
{
    template <typename T>
    const T *getComp(flecs::world &w, flecs::entity e)
    {
        return static_cast<const T *>(
            ecs_get_id(w.c_ptr(), e.raw_id(), w.component<T>().raw_id()));
    }
} // namespace

class ScriptSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Input::instance().reset();
        m_engine = new Engine();
        m_script = new ScriptSystem();
        m_script->init(m_engine->world());
    }
    void TearDown() override
    {
        // ★ 顺序关键：先 shutdown（m_impl 置空）→ 再销毁 world →
        //   最后 delete system。world 析构会触发 ScriptComponent 的
        //   OnRemove observer（回调捕获了 system 的 this）——若先 delete
        //   system 就是 UAF（0xc0000005，实测踩过）。shutdown 后回调里
        //   的 m_impl 判空防护会安全返回。
        m_script->shutdown();
        delete m_engine; // world 销毁，observer 触发（安全：m_impl 已空）
        delete m_script;
    }

    // 建一个挂内嵌脚本的实体（source 直接写 Lua 代码，不依赖文件）
    flecs::entity makeScripted(const char *name, const char *luaSrc)
    {
        auto e = m_engine->world().entity();
        TagComponent tc{};
        strncpy(tc.name, name, sizeof(tc.name) - 1);
        e.set<TagComponent>(tc);
        e.set<TransformComponent>({});
        ScriptComponent sc{};
        strncpy(sc.source, luaSrc, sizeof(sc.source) - 1);
        e.set<ScriptComponent>(sc);
        return e;
    }

    void stepFrames(int n, float dt = 1.0f / 60.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            m_script->update(m_engine->world(), dt);
            Input::instance().endFrame();
        }
    }

    Engine *m_engine = nullptr;
    ScriptSystem *m_script = nullptr;
};

// ---- 生命周期 ----

TEST_F(ScriptSystemTest, Ready_RunsExactlyOnce)
{
    // ready 每次执行 x+1 —— 若跑多次 x 会 >1
    auto e = makeScripted("A",
                          "function ready() local x,y,z = get_position() set_position(x+1, y, z) end");
    stepFrames(3);
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_FLOAT_EQ(tf->position.x, 1.0f); // 只跑了一次
}

TEST_F(ScriptSystemTest, Update_RunsEveryFrame)
{
    auto e = makeScripted("A",
                          "function update(dt) local x,y,z = get_position() set_position(x+1, y, z) end");
    stepFrames(5);
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_FLOAT_EQ(tf->position.x, 5.0f); // 每帧 +1，5 帧 = 5
}

TEST_F(ScriptSystemTest, Update_ReceivesDeltaTime)
{
    auto e = makeScripted("A",
                          "function update(dt) set_position(dt, 0, 0) end");
    stepFrames(1, 0.25f); // 传特定 dt
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_NEAR(tf->position.x, 0.25f, 1e-5f);
}

// 回归：dt=0 的帧（两帧挤同一毫秒）update 必须收到数字 0 而非 nil。
// 修复前 callLifecycle 用 dt>0 判断传不传参 → dt=0 时脚本拿到 nil，
// 对 dt 做算术直接崩（竞技场 AI 实测炸出）
TEST_F(ScriptSystemTest, Update_ZeroDt_StillReceivesNumber)
{
    auto e = makeScripted("A",
                          "function update(dt) set_position(1 + dt, 0, 0) end"); // dt 为 nil 会算术错
    stepFrames(1, 0.0f);                                                         // dt = 0 的帧
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_FLOAT_EQ(tf->position.x, 1.0f); // 正常执行了（没因 nil 崩掉）
}

// ---- 沙盒隔离 ----

TEST_F(ScriptSystemTest, Sandbox_TwoEntitiesSameSource_LocalsIsolated)
{
    // 同一份源码：script 级 local counter。若共享环境，第二个实体会看到
    // 第一个实体累加过的 counter → 位置错
    const char *src =
        "local counter = 0\n"
        "function update(dt) counter = counter + 1 set_position(counter, 0, 0) end";
    auto a = makeScripted("A", src);
    auto b = makeScripted("B", src);
    stepFrames(3);
    // 各自独立累加到 3（不是 6）
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), a)->position.x, 3.0f);
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), b)->position.x, 3.0f);
}

// ---- 坑26 回归：resetAllScripts 后 ready 重跑、local 归零 ----

TEST_F(ScriptSystemTest, ResetAllScripts_ReadyRunsAgainAndLocalsReset)
{
    auto e = makeScripted("A",
                          "local inited = 0\n"
                          "function ready() inited = inited + 1 end\n"
                          "function update(dt) set_position(inited, 0, 0) end");
    stepFrames(2);
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), e)->position.x, 1.0f);

    m_script->resetAllScripts(); // = 退出 Play 的清理
    stepFrames(2);               // = 第二次 Play
    // 环境重建：inited 归零后 ready 又 +1 → 仍是 1（坑26 修复前 ready 不跑，
    // 且 local 残留——位置行为完全错乱）
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), e)->position.x, 1.0f);
}

// ---- 脚本 API 与 Input 集成 ----

TEST_F(ScriptSystemTest, IsKeyJustPressed_EdgeVisibleToScriptOneFrame)
{
    auto e = makeScripted("A",
                          "function update(dt)\n"
                          "  if is_key_just_pressed('W') then\n"
                          "    local x,y,z = get_position() set_position(x+1, y, z)\n"
                          "  end\n"
                          "end");
    Input::instance().onKey('W', true); // 按下
    stepFrames(3);                      // 边沿只活第一帧（endFrame 清）
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), e)->position.x, 1.0f);
}

TEST_F(ScriptSystemTest, GetPosition_ReadsTransformSetByCpp)
{
    auto e = makeScripted("A",
                          "function update(dt) local x,y,z = get_position() set_position(x, y, x*2) end");
    // C++ 侧先改位置 → 脚本读到并写 z = x*2
    m_engine->world().entity(e.raw_id()).set<TransformComponent>({{7, 0, 0}});
    stepFrames(1);
    const auto *tf = getComp<TransformComponent>(m_engine->world(), e);
    EXPECT_FLOAT_EQ(tf->position.z, 14.0f);
}

// ---- 异常路径：坏脚本不拖垮系统 ----

TEST_F(ScriptSystemTest, BrokenScript_DoesNotAffectOtherEntities)
{
    makeScripted("Bad", "this is not valid lua !!!");
    auto good = makeScripted("Good",
                             "function update(dt) local x,y,z = get_position() set_position(x+1, y, z) end");
    stepFrames(3); // 坏脚本编译失败被跳过，好脚本照常跑
    EXPECT_FLOAT_EQ(getComp<TransformComponent>(m_engine->world(), good)->position.x, 3.0f);
}

// ---- 坑P1 回归：监听者死亡后，源实体的信号连接被清理（无孤儿回调）----
// 同时隐含验证：connect_to 在脚本执行中给目标实体首次加 SignalComponent
// 不再触发 flecs 表锁崩溃（ScriptSystem 两阶段执行修复）

TEST_F(ScriptSystemTest, EntityDeath_CleansUpSignalConnections)
{
    // listener connect_to emitter 的 "ping" 信号（emitter 尚无 SignalComponent，
    // 由 connect_to 在脚本执行中自动添加——曾触发 LOCKED_STORAGE assert）
    auto emitter = makeScripted("Emitter", "function update(dt) end");
    flecs::entity_t emitterId = emitter.raw_id();
    auto listener = makeScripted("Listener",
                                 ("function ready() connect_to(" + std::to_string(emitterId) +
                                  ", 'ping', function() end) end\nfunction update(dt) end")
                                     .c_str());
    stepFrames(2); // ready 跑完，连接已建立

    // emitter 的 signalMap["ping"] 现在应有 1 个来自 listener 的连接
    const auto *sigBefore = getComp<SignalComponent>(m_engine->world(), emitter);
    ASSERT_NE(sigBefore, nullptr);
    auto itBefore = sigBefore->signalMap.find("ping");
    ASSERT_NE(itBefore, sigBefore->signalMap.end());
    EXPECT_EQ(itBefore->second.size(), 1u);

    // ★ 销毁 listener → 触发 SignalComponent OnRemove → cleanupEntitySignals
    //   应去 emitter.signalMap["ping"] 删掉 listenerId==listener 的连接
    m_engine->world().entity(listener.raw_id()).destruct();
    stepFrames(1);

    const auto *sigAfter = getComp<SignalComponent>(m_engine->world(), emitter);
    ASSERT_NE(sigAfter, nullptr);
    auto itAfter = sigAfter->signalMap.find("ping");
    // 连接已被清空（要么 key 没了，要么列表空）——不再有指向死亡监听者的孤儿回调
    bool cleaned = (itAfter == sigAfter->signalMap.end()) || itAfter->second.empty();
    EXPECT_TRUE(cleaned);
}
