#ifndef RENDER_VISIBILITY_H
#define RENDER_VISIBILITY_H

#include <array>
#include <cmath>
#include <glm/glm.hpp>

// Mesh-local bounds become world bounds using the full parent-composed matrix.
// The center/extent form handles rotation, negative scale and nonuniform scale.
struct RenderAabb
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

inline RenderAabb transformRenderAabb(const RenderAabb &local, const glm::mat4 &world)
{
    const glm::vec3 center = (local.min + local.max) * 0.5f;
    const glm::vec3 extent = (local.max - local.min) * 0.5f;
    const glm::vec3 worldCenter = glm::vec3(world * glm::vec4(center, 1.0f));
    const glm::vec3 worldExtent =
        glm::abs(glm::vec3(world[0])) * extent.x +
        glm::abs(glm::vec3(world[1])) * extent.y +
        glm::abs(glm::vec3(world[2])) * extent.z;
    return {worldCenter - worldExtent, worldCenter + worldExtent};
}

class RenderFrustum
{
public:
    static RenderFrustum fromViewProjection(const glm::mat4 &vp)
    {
        RenderFrustum result;
        const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
        const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
        const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
        const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);
        result.m_planes = {row3 + row0, row3 - row0,
                           row3 + row1, row3 - row1,
                           row3 + row2, row3 - row2};
        for (auto &plane : result.m_planes)
        {
            const float length = glm::length(glm::vec3(plane));
            if (!std::isfinite(length) || length < 1e-6f)
            {
                result.m_valid = false; // Invalid camera matrix: draw conservatively.
                return result;
            }
            plane /= length;
        }
        result.m_valid = true;
        return result;
    }

    bool intersects(const RenderAabb &bounds) const
    {
        if (!m_valid) return true;
        const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
        const glm::vec3 extent = (bounds.max - bounds.min) * 0.5f;
        for (const auto &plane : m_planes)
        {
            const glm::vec3 normal(plane);
            const float radius = glm::dot(glm::abs(normal), extent);
            if (glm::dot(normal, center) + plane.w + radius < -1e-4f)
                return false;
        }
        return true;
    }

private:
    std::array<glm::vec4, 6> m_planes{};
    bool m_valid = false;
};

#endif
