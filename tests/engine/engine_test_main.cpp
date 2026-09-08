// engine_test_main.cpp - Headless engine test (no Qt GUI, no GL)
// Decoupled: delete tests/engine/ + CMake target to remove entirely.

#include <cstdio>
#include <cstring>
#include <string>

#pragma push_macro("emit")
#undef emit
#pragma push_macro("signals")
#undef signals
#include <flecs.h>
#pragma pop_macro("signals")
#pragma pop_macro("emit")

#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/EngineLog.h"
#include "Core/EngineState.h"
#include "Core/System.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/SignalComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/CameraComponent.h"
#include "Components/LightComponent.h"
#include "Components/EnvironmentComponent.h"
#include "Components/ParticleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/EditorVisible.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/TweenSystem.h"
#include "Scripting/LuaApi.h"
#include "Generated/ComponentReflection.inl"

// Test API (LuaTestApi.cpp)
extern void registerTestApis(lua_State *L);

// Bridge functions for LuaTestApi.cpp
static flecs::world *g_world = nullptr;
static PhysicsSystem *g_physics = nullptr;
static ScriptSystem *g_script = nullptr;
static TweenSystem *g_tween = nullptr;

void headlessRunFrames(int count, float dt)
{
    if (!g_world)
        return;
    for (int i = 0; i < count; ++i)
    {
        Time::instance().step(dt);
        g_tween->update(*g_world, dt);
        g_physics->update(*g_world, dt);
        g_script->update(*g_world, dt);
        Input::instance().endFrame();
    }
}

flecs::world *headlessGetWorld() { return g_world; }

int main(int argc, char *argv[])
{
    printf("=== Engine Headless Test ===\n");

    // Set test API hook BEFORE ScriptSystem::init()
    setScriptExtraApiHook(registerTestApis);

    // Create world + register components
    flecs::world world;
    g_world = &world;

    world.component<TransformComponent>();
    world.component<TagComponent>();
    world.component<MeshComponent>();
    world.component<MaterialComponent>();
    world.component<RigidbodyComponent>();
    world.component<BoxColliderComponent>();
    world.component<ScriptComponent>();
    world.component<SignalComponent>();
    world.component<AnimationComponent>();
    world.component<CameraComponent>();
    world.component<LightComponent>();
    world.component<EnvironmentComponent>();
    world.component<ParticleComponent>();
    world.component<HealthComponent>();
    world.component<SpriteComponent>();
    world.component<RotateBehavior>();
    world.component<EditorVisible>();
    world.component<glm::vec3>()
        .member<float>("x", 1, offsetof(glm::vec3, x))
        .member<float>("y", 1, offsetof(glm::vec3, y))
        .member<float>("z", 1, offsetof(glm::vec3, z));
    registerGeneratedReflection(world);

    // Init systems
    PhysicsSystem physics;
    physics.init();
    g_physics = &physics;

    TweenSystem tweenSys;
    g_tween = &tweenSys;

    ScriptSystem script;
    script.init(world);
    g_script = &script;

    // Test script path
    std::string testScript;
    if (argc > 1)
        testScript = argv[1];
    else
        testScript = std::string(ASSET_DIR) + "scripts/tests/run_all.lua";
    printf("[INFO] Script: %s\n", testScript.c_str());

    // Create test entity
    auto e = world.entity();
    TagComponent tag{};
    strncpy(tag.name, "TestRunner", sizeof(tag.name) - 1);
    e.set<TagComponent>(tag);
    e.set<TransformComponent>({});
    ScriptComponent sc{};
    strncpy(sc.filePath, testScript.c_str(), sizeof(sc.filePath) - 1);
    e.set<ScriptComponent>(sc);

    // Run (test script executes within update loop)
    headlessRunFrames(600, 1.0f / 60.0f);

    // Cleanup
    script.shutdown();
    physics.shutdown();
    shutdownEngineLog();
    g_world = nullptr;

    printf("=== Done ===\n");
    return 0;
}
