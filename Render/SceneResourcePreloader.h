#ifndef SCENE_RESOURCE_PRELOADER_H
#define SCENE_RESOURCE_PRELOADER_H

#include "Render/ResourceManager.h"
#include "Components/MeshComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/MaterialComponent.h"
#include <flecs.h>

// Called after a scene has been loaded and, in packaged games, paths rebased.
// Only CPU jobs are queued here; GL uploads are limited in paintGL().
inline void preloadSceneResources(flecs::world &world)
{
    auto &resources = ResourceManager::instance();
    resources.cancelPendingLoads();
    world.each([&](const MeshComponent &mesh)
               { resources.preloadMesh(mesh.meshHandle); });
    world.each([&](SpriteComponent &sprite)
    {
        if (!sprite.folder[0]) return;
        if (!sprite.spriteHandle)
            sprite.spriteHandle = resources.acquireSprite(sprite.folder);
        resources.preloadSpriteFrame(sprite.spriteHandle, sprite.frame);
    });
    world.each([&](MaterialComponent &material)
    {
        if (!material.albedoTex[0]) return;
        if (!material.texHandle)
            material.texHandle = resources.acquireMaterialTexture(material.albedoTex);
        resources.preloadTexture(material.texHandle);
    });
}

#endif
