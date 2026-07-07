#ifndef GAME_ENGINE_ENGINE_H
#define GAME_ENGINE_ENGINE_H

#include "ecs/world.h"
#include "render/renderer.h"
#include "render/mesh_factory.h"

#include <string>
#include <unordered_set>

// ============================================================
// Engine — 门面类（Facade）
//
// 对编辑器暴露最简 API，内部协调 World + Renderer 两大子系统。
// 组件设置通过显式重载实现（因为 MSVC C++20 下模板参数无法用于 Flecs 宏展开）。
// ============================================================
class Engine
{
public:
    // ---- 初始化 ----
    bool init(const std::string &shaderPath)
    {
        return m_renderer.initShaders(shaderPath);
    }

    // ---- 子系统访问 ----
    EngineWorld &world() { return m_world; }
    Renderer &renderer() { return m_renderer; }
    MeshFactory &meshFactory() { return m_renderer.meshFactory(); }

    // ---- 实体操作 ----
    ecs_entity_t createEntity(const std::string &name = "Entity")
    {
        return m_world.createEntity(name);
    }

    // ---- 组件设置（显式重载, 每个组件一个函数） ----
    void setComponent(ecs_entity_t e, const TransformComponent &v)
    {
        ecs_set_ptr(m_world.raw(), e, TransformComponent, &v);
    }
    void setComponent(ecs_entity_t e, const MeshComponent &v)
    {
        ecs_set_ptr(m_world.raw(), e, MeshComponent, &v);
    }
    void setComponent(ecs_entity_t e, const MaterialComponent &v)
    {
        ecs_set_ptr(m_world.raw(), e, MaterialComponent, &v);
    }
    void setComponent(ecs_entity_t e, const LightComponent &v)
    {
        ecs_set_ptr(m_world.raw(), e, LightComponent, &v);
    }
    void setComponent(ecs_entity_t e, const CameraComponent &v)
    {
        ecs_set_ptr(m_world.raw(), e, CameraComponent, &v);
    }

    // ---- 资源清理（必须在 GL 上下文有效时调用） ----
    void cleanupGL()
    {
        auto entities = m_world.getAllEntities();
        std::unordered_set<GLuint> deleted;

        for (auto e : entities)
        {
            const auto *m = static_cast<const MeshComponent *>(ecs_get(m_world.raw(), e, MeshComponent));
            if (m && m->vao && deleted.find(m->vao) == deleted.end())
            {
                deleted.insert(m->vao);
                m_renderer.meshFactory().destroyMesh(const_cast<MeshComponent &>(*m));
            }
        }
    }

private:
    EngineWorld m_world;
    Renderer m_renderer;
};

#endif // GAME_ENGINE_ENGINE_H
#ifndef GAME_ENGINE_ENGINE_H
#define GAME_ENGINE_ENGINE_H

#include "ecs/world.h"
#include "render/renderer.h"
#include "render/mesh_factory.h"
#include <unordered_set>

class Engine
{
public:
    bool init(const QString &shaderPath) { return m_renderer.initShaders(shaderPath); }
    EngineWorld &world() { return m_world; }
    Renderer &renderer() { return m_renderer; }
    MeshFactory &meshFactory() { return m_renderer.meshFactory(); }

    ecs_entity_t createEntity(const QString &name = "Entity") { return m_world.createEntity(name); }

    void setComponent(ecs_entity_t e, const TransformComponent &v) { ecs_set_ptr(m_world.raw(), e, TransformComponent, &v); }
    void setComponent(ecs_entity_t e, const MeshComponent &v) { ecs_set_ptr(m_world.raw(), e, MeshComponent, &v); }
    void setComponent(ecs_entity_t e, const MaterialComponent &v) { ecs_set_ptr(m_world.raw(), e, MaterialComponent, &v); }
    void setComponent(ecs_entity_t e, const LightComponent &v) { ecs_set_ptr(m_world.raw(), e, LightComponent, &v); }
    void setComponent(ecs_entity_t e, const CameraComponent &v) { ecs_set_ptr(m_world.raw(), e, CameraComponent, &v); }

    void cleanupGL()
    {
        auto entities = m_world.getAllEntities();
        std::unordered_set<GLuint> deleted;
        for (auto e : entities)
        {
            auto *m = (MeshComponent *)ecs_get(m_world.raw(), e, MeshComponent);
            if (m && m->vao && deleted.find(m->vao) == deleted.end())
            {
                deleted.insert(m->vao);
                m_renderer.meshFactory().destroyMesh(const_cast<MeshComponent &>(*m));
            }
        }
    }

private:
    EngineWorld m_world;
    Renderer m_renderer;
};

#endif
