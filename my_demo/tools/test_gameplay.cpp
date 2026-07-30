// ============================================================
// test_gameplay.cpp — 玩家控制链路最小复现测试（无 GUI 控制台）
//
// 完整链路：Input.onKey('W') → Engine(Play) → ScriptSystem(player.lua)
//           → set_velocity → Jolt → PhysicsSystem 回写 Transform
//
// 分层诊断（哪层断了一目了然）：
//   ① 物理体创建：rb.bodyId != 0 ?
//   ② 重力生效：y 从 3 掉到 ~0.5 ?（脚本死了也该掉——物理段独立验证）
//   ③ 脚本推动：按 W 后 z 是否变负 ?（脚本段验证）
//   ④ 跳跃：落地后模拟空格，y 是否弹起 ?
// ============================================================
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/SaveGame.h" // 探针2：Lua→C++ 诊断回传通道
#include "Systems/RotationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Render/SceneSerializer.h"
#include "Components/TagComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/TransformComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/CameraComponent.h"
#include "Components/RotateBehavior.h"

#include <cstdio>
#include <cstring>

// 坑 2 防踩：e.get<T>() 在 MSVC 会崩，统一用 ecs_get_id
template <typename T>
static const T *getComp(flecs::entity e)
{
    return static_cast<const T *>(ecs_get_id(
        e.world().c_ptr(), e.raw_id(), e.world().component<T>().raw_id()));
}

int main()
{
    printf("=== gameplay chain test ===\n");

    Engine engine;
    engine.addSystem(std::make_unique<RotationSystem>());
    engine.addSystem(std::make_unique<CameraSystem>());
    engine.addSystem(std::make_unique<TransformSystem>());
    engine.addSystem(std::make_unique<AnimationSystem>());
    auto *phys = new PhysicsSystem;
    phys->init();
    engine.addSystem(std::unique_ptr<PhysicsSystem>(phys));
    auto *script = new ScriptSystem;
    script->init(engine.world());
    engine.addSystem(std::unique_ptr<ScriptSystem>(script));

    engine.context().onEnterPlay = [&]()
    { engine.onEnterPlay(); };
    engine.context().onExitPlay = [&]()
    { engine.onExitPlay(); };

    // ★ 程序化建场景（用引擎 API，不手写 JSON）——手写 [x,y,z] 或
    // {x,y,z} 都不匹配 flecs 期望的嵌套格式，导致反序列化全部变 0。
    // 正确做法：建实体 → SceneSerializer::save 生成引擎自己认的格式。
    {
        auto &w = engine.world();
        auto mkCube = [&](const char *name, glm::vec3 pos, glm::vec3 scl,
                          glm::vec3 color, int motion, const char *script)
        {
            auto e = w.entity();
            TagComponent tc{};
            strncpy(tc.name, name, sizeof(tc.name) - 1);
            e.set<TagComponent>(tc);
            TransformComponent tf{};
            tf.position = pos;
            tf.scale = scl;
            e.set<TransformComponent>(tf);
            MaterialComponent mat{};
            mat.diffuse = color;
            e.set<MaterialComponent>(mat);
            MeshComponent m{};
            strncpy(m.meshType, "cube", sizeof(m.meshType) - 1);
            e.set<MeshComponent>(m);
            if (motion >= 0) // motion=-1 纯装饰（传送门）：无碰撞体
            {
                RigidbodyComponent rb{};
                rb.motionType = motion;
                e.set<RigidbodyComponent>(rb);
                e.set<BoxColliderComponent>({});
            }
            if (script)
            {
                ScriptComponent sc{};
                strncpy(sc.filePath, script, sizeof(sc.filePath) - 1);
                e.set<ScriptComponent>(sc);
            }
            return e;
        };
        auto mkCamera = [&](glm::vec3 pos, float yaw, float pitch)
        {
            auto camE = w.entity();
            TagComponent ct{};
            strncpy(ct.name, "GameCamera", sizeof(ct.name) - 1);
            camE.set<TagComponent>(ct);
            camE.set<TransformComponent>({});
            CameraComponent cam{};
            cam.position = pos;
            cam.yaw = yaw;
            cam.pitch = pitch;
            cam.isEditor = false;
            cam.active = true;
            camE.set<CameraComponent>(cam);
        };
        auto clearScene = [&]()
        {
            std::vector<flecs::entity_t> del;
            w.each([&](flecs::entity e, TagComponent &)
                   { del.push_back(e.raw_id()); });
            for (auto id : del)
                if (w.is_alive(id))
                    w.entity(id).destruct();
        };
        const char *playerLua = "D:/code/.std/opengl/my_demo/scripts/player.lua";

        // ======== Level 1：绿地场（箱子 + 金色旋转传送门） ========
        // 贴地摆放：地面顶面 y=0，1x1 盒子半高 0.5 → 中心 y=0.55
        // （留 0.05 缝隙防浮点穿插；出生在空中会"开场跳水"，实反馈）
        mkCube("Player", {0, 0.55f, 0}, {1, 1, 1}, {1.0f, 0.55f, 0.15f}, 2, playerLua);
        mkCube("Ground", {0, -0.5f, 0}, {20, 1, 20}, {0.35f, 0.5f, 0.35f}, 0, nullptr);
        mkCube("Box1", {3, 0.55f, -2}, {1, 1, 1}, {0.3f, 0.5f, 0.9f}, 2, nullptr);
        mkCube("Box2", {-3, 0.55f, -3}, {1, 1, 1}, {0.9f, 0.3f, 0.35f}, 2, nullptr);
        {
            // 金色传送门：纯装饰（无碰撞）+ 自转；玩家靠近即传送（脚本距离检测）
            auto portal = mkCube("Portal", {0, 1, -7}, {0.8f, 1.6f, 0.3f},
                                 {1.0f, 0.85f, 0.1f}, -1, nullptr);
            RotateBehavior rot{};
            rot.speedY = 90;
            portal.set<RotateBehavior>(rot);
        }
        mkCamera({0, 9, 14}, -90, -30);
        bool sv1 = SceneSerializer::save(w, "d:/code/.std/opengl/my_demo/scenes/level1.json");
        printf("level1 saved: %s\n", sv1 ? "OK" : "FAIL");

        // ======== Level 2：紫地台（阶梯平台 + 绿色回程门） ========
        clearScene();
        mkCube("Player", {0, 0.55f, 5}, {1, 1, 1}, {1.0f, 0.55f, 0.15f}, 2, playerLua);
        mkCube("Ground", {0, -0.5f, 0}, {14, 1, 14}, {0.45f, 0.35f, 0.6f}, 0, nullptr);
        mkCube("Step1", {2, 0.5f, 1}, {2, 1, 2}, {0.6f, 0.5f, 0.8f}, 0, nullptr);
        mkCube("Step2", {-1, 1.2f, -1}, {2, 1, 2}, {0.6f, 0.5f, 0.8f}, 0, nullptr);
        mkCube("Step3", {2, 2.0f, -3}, {2, 1, 2}, {0.6f, 0.5f, 0.8f}, 0, nullptr);
        {
            // 绿色回程门（最高平台上）：回 level1
            auto portal = mkCube("Portal", {2, 3.6f, -3}, {0.8f, 1.6f, 0.3f},
                                 {0.2f, 1.0f, 0.4f}, -1, nullptr);
            RotateBehavior rot{};
            rot.speedY = 90;
            portal.set<RotateBehavior>(rot);
        }
        mkCamera({0, 8, 13}, -90, -30);
        bool sv2 = SceneSerializer::save(w, "d:/code/.std/opengl/my_demo/scenes/level2.json");
        printf("level2 saved: %s\n", sv2 ? "OK" : "FAIL");

        // ======== code_world：纯代码造世界的极简场景 ========
        // 只有一个挂 bootstrap 脚本的空实体——地面/玩家/箱子/相机
        // 全部由 code_world.lua 在 ready() 里用建造 API 搭出来
        clearScene();
        {
            auto boot = w.entity();
            TagComponent tc{};
            strncpy(tc.name, "Main", sizeof(tc.name) - 1);
            boot.set<TagComponent>(tc);
            boot.set<TransformComponent>({}); // ScriptSystem 查询需要 Transform
            ScriptComponent sc{};
            strncpy(sc.filePath, "D:/code/.std/opengl/my_demo/scripts/code_world.lua",
                    sizeof(sc.filePath) - 1);
            boot.set<ScriptComponent>(sc);
        }
        bool sv3 = SceneSerializer::save(w, "d:/code/.std/opengl/my_demo/scenes/code_world.json");
        printf("code_world saved: %s\n", sv3 ? "OK" : "FAIL");

        // 链路测试用 level1（重新加载，验证与磁盘文件一致）
        clearScene();
        SceneSerializer::load(w, "d:/code/.std/opengl/my_demo/scenes/level1.json");
        printf("level1 reloaded for chain test\n");
    }

    // 找 Player
    flecs::entity player;
    engine.world().each([&](flecs::entity e, TagComponent &tag)
                        { if (strcmp(tag.name, "Player") == 0) player = e; });
    if (!player)
    {
        printf("Player NOT FOUND\n");
        return 1;
    }

    const auto *tf0 = getComp<TransformComponent>(player);
    printf("start pos: (%.2f, %.2f, %.2f)\n",
           tf0->position.x, tf0->position.y, tf0->position.z);

    engine.setPlay();
    printf("state after setPlay: %s\n",
           engine.state() == EngineState::Play ? "Play" : "NOT-PLAY!");
    fflush(stdout);

    // 逐帧打印帧号（定位崩溃在哪一帧）
    for (int i = 0; i < 30; ++i)
    {
        printf("frame %d\n", i);
        fflush(stdout);
        Time::instance().step(1.0f / 60.0f);
        engine.runFrame(1.0f / 60.0f);
        Input::instance().endFrame();
    }
    const auto *rb = getComp<RigidbodyComponent>(player);
    const auto *tf1 = getComp<TransformComponent>(player);
    printf("after loop: rb=%p tf1=%p\n", (void *)rb, (void *)tf1);
    fflush(stdout);
    printf("[1] bodyId=%u %s\n", rb ? rb->bodyId : 0u,
           (rb && rb->bodyId) ? "OK(body created)" : "FAIL(no body!)");
    printf("[2] after 30 frames no-input: y=%.2f %s\n", tf1 ? tf1->position.y : -999.0f,
           (tf1 && tf1->position.y < 2.0f) ? "OK(gravity works)" : "FAIL(not falling!)");
    fflush(stdout);

    // 按住 W 跑 60 帧 → 期望 z 变负（player.lua: W → mz=-5）
    Input::instance().onKey('W', true);
    for (int i = 0; i < 60; ++i)
    {
        printf("W-frame %d\n", i);
        fflush(stdout);
        Time::instance().step(1.0f / 60.0f);
        engine.runFrame(1.0f / 60.0f);
        Input::instance().endFrame();
    }
    Input::instance().onKey('W', false);
    printf("player alive=%d\n", engine.world().is_alive(player.raw_id()) ? 1 : 0);
    fflush(stdout);
    const auto *tf2 = getComp<TransformComponent>(player);
    printf("tf2=%p\n", (void *)tf2);
    fflush(stdout);
    printf("[3] after 60 frames W held: z=%.2f %s\n", tf2 ? tf2->position.z : -999.0f,
           (tf2 && tf2->position.z < -0.5f) ? "OK(script moves player)" : "FAIL(script chain broken!)");
    fflush(stdout);

    // 空格跳：模拟按下（边沿在下一帧被脚本读到后 endFrame 清除）
    float yBefore = tf2 ? tf2->position.y : 0;
    Input::instance().onKey(0x20, true); // Qt::Key_Space
    for (int i = 0; i < 15; ++i)
    {
        Time::instance().step(1.0f / 60.0f);
        engine.runFrame(1.0f / 60.0f);
        Input::instance().endFrame();
    }
    const auto *tf3 = getComp<TransformComponent>(player);
    printf("[4] after space: y %.2f -> %.2f %s\n", yBefore, tf3 ? tf3->position.y : -999.0f,
           (tf3 && tf3->position.y > yBefore + 0.2f) ? "OK(jump works)" : "FAIL(no jump)");
    fflush(stdout);

    engine.context() = EngineContext{}; // 防析构回调 UAF
    printf("=== done ===\n");
    return 0;
}
