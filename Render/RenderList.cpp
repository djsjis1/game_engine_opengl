#include "Render/RenderList.h"
#include "Render/ResourceManager.h"
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"

void RenderList::build(flecs::world &world)
{
    m_items.clear();
    world.each([this](flecs::entity entity, const TransformComponent &transform,
                      const MeshComponent &mesh)
    {
        if (!mesh.isValid() || !transform.visibleInTree) return;
        const GpuMesh *gpu = ResourceManager::instance().get(mesh.meshHandle);
        if (!gpu) return; // Async OBJ is still loading.
        RenderItem item;
        item.entity = entity.raw_id();
        item.model = transform.worldMatrix;
        item.normal = transform.worldNormalMatrix;
        item.mesh = *gpu; // GL object IDs only; ResourceManager keeps ownership.
        item.worldBounds = transformRenderAabb({gpu->boundsMin, gpu->boundsMax},
                                               transform.worldMatrix);
        m_items.push_back(item);
    });
}
