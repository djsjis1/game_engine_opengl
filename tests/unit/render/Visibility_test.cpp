#include <gtest/gtest.h>
#include "Render/Visibility.h"
#include <glm/gtc/matrix_transform.hpp>

TEST(RenderVisibilityTest, RotationAndParentWorldTransformExpandBounds)
{
    const RenderAabb cube{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    const glm::mat4 parent = glm::translate(glm::mat4(1.0f), {3.0f, 0.0f, -5.0f});
    const glm::mat4 child = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f),
                                        {0.0f, 0.0f, 1.0f}) *
                            glm::scale(glm::mat4(1.0f), {-4.0f, 1.0f, 1.0f});
    const RenderAabb world = transformRenderAabb(cube, parent * child);
    EXPECT_NEAR((world.min.x + world.max.x) * 0.5f, 3.0f, 1e-5f);
    EXPECT_GT(world.max.x - world.min.x, 3.5f);
    EXPECT_NEAR((world.min.z + world.max.z) * 0.5f, -5.0f, 1e-5f);
}

TEST(RenderVisibilityTest, CullsOutsideButKeepsIntersectingBounds)
{
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 50.0f);
    const RenderFrustum frustum = RenderFrustum::fromViewProjection(projection);
    const RenderAabb cube{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    const glm::mat4 visibleWorld = glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, -5.0f});
    const glm::mat4 outsideWorld = glm::translate(glm::mat4(1.0f), {100.0f, 0.0f, -5.0f});
    const glm::mat4 behindWorld = glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 5.0f});
    EXPECT_TRUE(frustum.intersects(transformRenderAabb(cube, visibleWorld)));
    EXPECT_FALSE(frustum.intersects(transformRenderAabb(cube, outsideWorld)));
    EXPECT_FALSE(frustum.intersects(transformRenderAabb(cube, behindWorld)));

    // A large object whose center is outside still intersects the view.
    const RenderAabb wide{{-10.0f, -0.5f, -0.5f}, {10.0f, 0.5f, 0.5f}};
    const glm::mat4 edgeWorld = glm::translate(glm::mat4(1.0f), {10.0f, 0.0f, -5.0f});
    EXPECT_TRUE(frustum.intersects(transformRenderAabb(wide, edgeWorld)));
}
