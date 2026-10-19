#pragma push_macro("emit")
#undef emit
#pragma push_macro("signals")
#undef signals
#include <flecs.h>
#pragma pop_macro("signals")
#pragma pop_macro("emit")

#include "Components/AudioComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SignalComponent.h"
#include "Components/SpriteComponent.h"

#include "Systems/AudioSystem.h"
#include "Render/ResourceManager.h"

// Implemented in the scripting module to keep Lua's full headers out of Engine.
void cleanupEntitySignals(flecs::world world, flecs::entity_t deadId);

void registerEngineObservers(flecs::world &world)
{
    // Resource references are released for every removal path, including
    // component removal and cascading entity destruction.
    world.observer<MeshComponent>()
        .event(flecs::OnRemove)
        .each([](flecs::entity, MeshComponent &mesh)
              { ResourceManager::instance().releaseMesh(mesh.meshHandle); });

    world.observer<MaterialComponent>()
        .event(flecs::OnRemove)
        .each([](flecs::entity, MaterialComponent &material)
              { ResourceManager::instance().releaseTexture(material.texHandle); });

    world.observer<SpriteComponent>()
        .event(flecs::OnRemove)
        .each([](flecs::entity, SpriteComponent &sprite)
              { ResourceManager::instance().releaseSprite(sprite.spriteHandle); });

    world.observer<AudioComponent>()
        .event(flecs::OnRemove)
        .each([](flecs::entity, AudioComponent &audio)
              { audioReleaseHandle(audio.soundHandle); });

    // OnRemove still exposes SignalComponent, so script registry references
    // can be unregistered before the component storage is reclaimed.
    world.observer<SignalComponent>()
        .event(flecs::OnRemove)
        .each([](flecs::entity entity, SignalComponent &)
              { cleanupEntitySignals(entity.world(), entity.raw_id()); });
}
