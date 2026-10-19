#include "Core/EngineSystems.h"
#include "Core/Engine.h"
#include "Systems/RotationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/ParticleSystem.h"
#include "Systems/TweenSystem.h"
#include "Systems/AnimFsmSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/AudioSystem.h"

DefaultSystems registerDefaultSystems(Engine &engine,
                                      ScriptSystem::HotReloadMode hotReloadMode)
{
    engine.addSystem(std::make_unique<RotationSystem>());
    engine.addSystem(std::make_unique<CameraSystem>());
    engine.addSystem(std::make_unique<TransformSystem>());
    engine.addSystem(std::make_unique<AnimationSystem>());
    engine.addSystem(std::make_unique<ParticleSystem>());
    engine.addSystem(std::make_unique<TweenSystem>());
    engine.addSystem(std::make_unique<AnimFsmSystem>());

    auto physics = std::make_unique<PhysicsSystem>();
    physics->init();
    PhysicsSystem *physicsHandle = physics.get();
    engine.addSystem(std::move(physics));

    auto script = std::make_unique<ScriptSystem>();
    script->setHotReloadMode(hotReloadMode);
    script->init(engine.world());
    ScriptSystem *scriptHandle = script.get();
    engine.addSystem(std::move(script));

    auto audio = std::make_unique<AudioSystem>();
    audio->init();
    AudioSystem *audioHandle = audio.get();
    engine.addSystem(std::move(audio));

    return {physicsHandle, scriptHandle, audioHandle};
}
