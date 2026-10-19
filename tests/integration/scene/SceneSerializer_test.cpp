#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QByteArray>
#include <cstring>

#include "Core/Engine.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/SphereColliderComponent.h"
#include "Components/CapsuleColliderComponent.h"
#include "Render/SceneSerializer.h"

namespace
{
    flecs::entity_t findByName(flecs::world &world, const char *name)
    {
        flecs::entity_t found = 0;
        world.each([&](flecs::entity e, const TagComponent &tag)
        {
            if (std::strcmp(tag.name, name) == 0)
                found = e.raw_id();
        });
        return found;
    }

    int countTaggedEntities(flecs::world &world)
    {
        int count = 0;
        world.each([&](flecs::entity, const TagComponent &) { ++count; });
        return count;
    }

    bool writeTempJson(QTemporaryFile &file, const QByteArray &contents)
    {
        if (!file.open())
            return false;
        if (file.write(contents) != contents.size() || !file.flush())
            return false;
        file.close();
        return true;
    }
}

TEST(SceneSerializerLoadTest, MissingEntitiesFieldKeepsCurrentScene)
{
    Engine engine;
    auto existing = engine.world().entity();
    TagComponent tag{};
    std::strncpy(tag.name, "existing", sizeof(tag.name) - 1);
    existing.set<TagComponent>(tag);
    existing.set<TransformComponent>({});

    QTemporaryFile file;
    ASSERT_TRUE(writeTempJson(file, R"({"version":1})"));

    EXPECT_FALSE(SceneSerializer::load(engine.world(), file.fileName().toStdString()));
    EXPECT_NE(findByName(engine.world(), "existing"), 0u);
    EXPECT_EQ(countTaggedEntities(engine.world()), 1);
}

TEST(SceneSerializerLoadTest, ParentCycleKeepsCurrentScene)
{
    Engine engine;
    auto existing = engine.world().entity();
    TagComponent tag{};
    std::strncpy(tag.name, "existing", sizeof(tag.name) - 1);
    existing.set<TagComponent>(tag);
    existing.set<TransformComponent>({});

    QTemporaryFile file;
    ASSERT_TRUE(writeTempJson(file,
        R"({"entities":[{"parent":1},{"parent":0}]})"));

    EXPECT_FALSE(SceneSerializer::load(engine.world(), file.fileName().toStdString()));
    EXPECT_NE(findByName(engine.world(), "existing"), 0u);
    EXPECT_EQ(countTaggedEntities(engine.world()), 1);
}

TEST(SceneSerializerLoadTest, ComponentDecodeFailureKeepsCurrentScene)
{
    Engine engine;
    auto existing = engine.world().entity();
    TagComponent tag{};
    std::strncpy(tag.name, "existing", sizeof(tag.name) - 1);
    existing.set<TagComponent>(tag);
    existing.set<TransformComponent>({});

    QTemporaryFile file;
    ASSERT_TRUE(writeTempJson(file,
        R"({"entities":[{"TagComponent":{"name":17}}]})"));

    EXPECT_FALSE(SceneSerializer::load(engine.world(), file.fileName().toStdString()));
    EXPECT_NE(findByName(engine.world(), "existing"), 0u);
    EXPECT_EQ(countTaggedEntities(engine.world()), 1);
}

TEST(SceneSerializerLoadTest, ValidSceneReplacesOldEntitiesAndRestoresHierarchy)
{
    Engine engine;
    auto root = engine.world().entity();
    TagComponent rootTag{};
    std::strncpy(rootTag.name, "loaded_root", sizeof(rootTag.name) - 1);
    root.set<TagComponent>(rootTag);
    root.set<TransformComponent>({});

    auto child = engine.world().entity();
    TagComponent childTag{};
    std::strncpy(childTag.name, "loaded_child", sizeof(childTag.name) - 1);
    child.set<TagComponent>(childTag);
    child.set<TransformComponent>({});
    child.child_of(root);

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const std::string path = tempDir.filePath("scene.json").toStdString();
    ASSERT_TRUE(SceneSerializer::save(engine.world(), path));
    ASSERT_TRUE(SceneSerializer::save(engine.world(), path)); // overwrite existing file atomically

    auto temporary = engine.world().entity();
    TagComponent temporaryTag{};
    std::strncpy(temporaryTag.name, "temporary", sizeof(temporaryTag.name) - 1);
    temporary.set<TagComponent>(temporaryTag);
    temporary.set<TransformComponent>({});

    ASSERT_TRUE(SceneSerializer::load(engine.world(), path));
    EXPECT_EQ(findByName(engine.world(), "temporary"), 0u);
    const flecs::entity_t loadedChildId = findByName(engine.world(), "loaded_child");
    ASSERT_NE(loadedChildId, 0u);
    const flecs::entity loadedChild = engine.world().entity(loadedChildId);
    const flecs::entity loadedParent = loadedChild.parent();
    ASSERT_TRUE(loadedParent && loadedParent.is_alive());
    const auto *loadedParentTag = static_cast<const TagComponent *>(ecs_get_id(
        engine.world().c_ptr(), loadedParent.raw_id(),
        engine.world().component<TagComponent>().raw_id()));
    ASSERT_NE(loadedParentTag, nullptr);
    EXPECT_STREQ(loadedParentTag->name, "loaded_root");
    EXPECT_EQ(countTaggedEntities(engine.world()), 2);
}

TEST(SceneSerializerLoadTest, RoundTripsSphereAndCapsuleColliderFields)
{
    Engine engine;
    auto sphereEntity = engine.world().entity();
    TagComponent sphereTag{};
    std::strncpy(sphereTag.name, "sphere", sizeof(sphereTag.name) - 1);
    sphereEntity.set<TagComponent>(sphereTag);
    sphereEntity.set<TransformComponent>({});
    sphereEntity.set<SphereColliderComponent>({1.25f, true});

    auto capsuleEntity = engine.world().entity();
    TagComponent capsuleTag{};
    std::strncpy(capsuleTag.name, "capsule", sizeof(capsuleTag.name) - 1);
    capsuleEntity.set<TagComponent>(capsuleTag);
    capsuleEntity.set<TransformComponent>({});
    capsuleEntity.set<CapsuleColliderComponent>({0.25f, 3.5f, false});

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const std::string path = tempDir.filePath("colliders.json").toStdString();
    ASSERT_TRUE(SceneSerializer::save(engine.world(), path));
    ASSERT_TRUE(SceneSerializer::load(engine.world(), path));

    const auto sphereId = findByName(engine.world(), "sphere");
    const auto capsuleId = findByName(engine.world(), "capsule");
    ASSERT_NE(sphereId, 0u);
    ASSERT_NE(capsuleId, 0u);
    const auto *sphere = static_cast<const SphereColliderComponent *>(ecs_get_id(
        engine.world().c_ptr(), sphereId, engine.world().component<SphereColliderComponent>().raw_id()));
    const auto *capsule = static_cast<const CapsuleColliderComponent *>(ecs_get_id(
        engine.world().c_ptr(), capsuleId, engine.world().component<CapsuleColliderComponent>().raw_id()));
    ASSERT_NE(sphere, nullptr);
    ASSERT_NE(capsule, nullptr);
    EXPECT_FLOAT_EQ(sphere->radius, 1.25f);
    EXPECT_TRUE(sphere->isTrigger);
    EXPECT_FLOAT_EQ(capsule->radius, 0.25f);
    EXPECT_FLOAT_EQ(capsule->height, 3.5f);
    EXPECT_FALSE(capsule->isTrigger);
}
