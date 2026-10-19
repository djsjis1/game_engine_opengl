#ifndef RENDER_LIST_H
#define RENDER_LIST_H

#include "Render/MeshFactory.h"
#include "Render/Visibility.h"
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#include <glm/glm.hpp>
#include <vector>

struct RenderItem
{
    flecs::entity_t entity = 0;
    glm::mat4 model{1.0f};
    glm::mat3 normal{1.0f};
    GpuMesh mesh;
    RenderAabb worldBounds;
};

// Reused each frame. All passes inspect the same ready mesh snapshot, avoiding
// repeated ECS mesh queries and resource lookups in point-shadow cube faces.
class RenderList
{
public:
    void build(flecs::world &world); // requires a current GL context
    const std::vector<RenderItem> &items() const { return m_items; }

private:
    std::vector<RenderItem> m_items;
};

#endif
