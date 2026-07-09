#ifndef ENGINE_H
#define ENGINE_H

#include <vector>

#ifndef Q_MOC_RUN
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#endif

#include "System/System.h"
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/TagComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/CameraComponent.h"
#include "Components/EditorVisible.h"
#include "Components/ScriptComponent.h"

class Engine
{
    void registerReflection()
    {
        m_world.component<glm::vec3>()
            .member<float>("x", offsetof(glm::vec3, x))
            .member<float>("y", offsetof(glm::vec3, y))
            .member<float>("z", offsetof(glm::vec3, z));

        m_world.component<TransformComponent>()
            .member<glm::vec3>("position", offsetof(TransformComponent, position))
            .member<glm::vec3>("rotation", offsetof(TransformComponent, rotation))
            .member<glm::vec3>("scale", offsetof(TransformComponent, scale));

        m_world.component<MaterialComponent>()
            .member<glm::vec3>("ambient", offsetof(MaterialComponent, ambient))
            .member<glm::vec3>("diffuse", offsetof(MaterialComponent, diffuse))
            .member<glm::vec3>("specular", offsetof(MaterialComponent, specular))
            .member<float>("shininess", offsetof(MaterialComponent, shininess));

        m_world.component<RotateBehavior>()
            .member<float>("speedX", offsetof(RotateBehavior, speedX))
            .member<float>("speedY", offsetof(RotateBehavior, speedY))
            .member<float>("speedZ", offsetof(RotateBehavior, speedZ));

        m_world.component<CameraComponent>()
            .member<float>("fov", offsetof(CameraComponent, fov))
            .member<float>("moveSpeed", offsetof(CameraComponent, moveSpeed))
            .member<bool>("active", offsetof(CameraComponent, active))
            .member<bool>("isEditor", offsetof(CameraComponent, isEditor));

        m_world.component<TransformComponent>().add<EditorVisible>();
        m_world.component<MeshComponent>().add<EditorVisible>();
        m_world.component<MaterialComponent>().add<EditorVisible>();
        m_world.component<RotateBehavior>().add<EditorVisible>();
        m_world.component<CameraComponent>().add<EditorVisible>();
        m_world.component<ScriptComponent>().add<EditorVisible>();
    }

public:
    Engine()
    {
        m_world.component<EditorVisible>();
        m_world.component<TransformComponent>();
        m_world.component<MeshComponent>();
        m_world.component<MaterialComponent>();
        m_world.component<TagComponent>();
        m_world.component<RotateBehavior>();
        m_world.component<CameraComponent>();
        m_world.component<ScriptComponent>();
        registerReflection();
    }

    void addSystem(System *sys) { m_systems.push_back(sys); }
    void runFrame(float dt)
    {
        for (auto *s : m_systems)
            s->update(m_world, dt);
    }
    flecs::world &world() { return m_world; }

private:
    flecs::world m_world;
    std::vector<System *> m_systems;
};

#endif
