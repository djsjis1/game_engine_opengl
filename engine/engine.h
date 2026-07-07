#ifndef ENGINE_ENGINE_H
#define ENGINE_ENGINE_H

#include <entt/entt.hpp>
#include "systems/camera_system.h"
#include "systems/light_system.h"
#include "systems/render_system.h"
#include "resources/shader_manager.h"
#include "resources/texture_manager.h"
#include "resources/mesh_factory.h"
#include <QString>
#include <QMatrix4x4>
#include <unordered_set>

// ============================================================
// Engine: 基于 EnTT 的轻量级渲染引擎
//
// 职责：
//   1. 持有 entt::registry（实体/组件容器）
//   2. 管理渲染系统、资源管理器
//   3. 提供统一的初始化/更新/渲染接口
//
// 与 EnTT 的关系：
//   - entt::registry 替代了手写的 Scene/ComponentManager
//   - 组件（TransformComponent 等）保持不变，直接用于 EnTT
//   - 系统类接收 registry&，用 view<T...>() 遍历实体
//
// 使用示例：
//   engine.init(shaderPath);
//   auto e = engine.createEntity();
//   engine.addComponent<TransformComponent>(e, {...});
//   engine.addComponent<MeshComponent>(e, mesh);
//   ...每帧: engine.update(dt); engine.render();
// ============================================================
class Engine
{
public:
    Engine() = default;
    ~Engine() = default;

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    // ---- 初始化 ----
    void init(const QString &shaderBasePath)
    {
        loadDefaultShaders(shaderBasePath);

        m_renderSystem.setLightApplyFunc([this](Shader *shader)
                                         { m_lightSystem.applyToShader(shader); });
    }

    // ---- 帧循环 ----
    void update(float dt)
    {
        m_cameraSystem.update(m_registry, dt);
        m_lightSystem.update(m_registry);

        // 同步摄像机数据到渲染系统
        m_renderSystem.setViewMatrix(m_cameraSystem.getViewMatrix());
        m_renderSystem.setProjectionMatrix(m_cameraSystem.getProjectionMatrix());
        m_renderSystem.setCameraPosition(m_cameraSystem.getCameraPosition());
    }

    void render()
    {
        m_renderSystem.render(m_registry);
    }

    // ---- Entity 操作 (EnTT 包装) ----
    entt::entity createEntity()
    {
        return m_registry.create();
    }

    void destroyEntity(entt::entity entity)
    {
        m_registry.destroy(entity);
    }

    template <typename T, typename... Args>
    T &addComponent(entt::entity entity, Args &&...args)
    {
        return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
    }

    template <typename T>
    T &getComponent(entt::entity entity)
    {
        return m_registry.get<T>(entity);
    }

    template <typename T>
    bool hasComponent(entt::entity entity)
    {
        return m_registry.all_of<T>(entity);
    }

    template <typename T>
    void removeComponent(entt::entity entity)
    {
        m_registry.remove<T>(entity);
    }

    // 获取 registry 引用（供系统或高级操作使用）
    entt::registry &registry() { return m_registry; }

    // ---- 资源管理器 ----
    ShaderManager &shaderManager() { return m_shaderManager; }
    TextureManager &textureManager() { return m_textureManager; }
    MeshFactory &meshFactory() { return m_meshFactory; }

    // ---- 系统访问 ----
    CameraSystem &cameraSystem() { return m_cameraSystem; }
    LightSystem &lightSystem() { return m_lightSystem; }
    RenderSystem &renderSystem() { return m_renderSystem; }

    // ---- 宽高比 ----
    void setAspectRatio(float ratio) { m_cameraSystem.setAspectRatio(ratio); }

    // ---- 清理 ----
    void cleanupGL()
    {
        // 遍历所有 MeshComponent，去重删除 VAO/VBO/EBO
        auto view = m_registry.view<MeshComponent>();
        std::unordered_set<GLuint> deletedVAOs;
        for (auto entity : view)
        {
            auto &mesh = view.get<MeshComponent>(entity);
            if (mesh.vao && deletedVAOs.find(mesh.vao) == deletedVAOs.end())
            {
                deletedVAOs.insert(mesh.vao);
                m_meshFactory.destroyMesh(mesh);
            }
        }
        m_textureManager.clear();
        m_shaderManager.clear();
    }

private:
    void loadDefaultShaders(const QString &basePath)
    {
        m_shaderManager.loadShader("default",
                                   basePath + "default.vert",
                                   basePath + "default.frag");
        m_shaderManager.loadShader("light_cube",
                                   basePath + "light_cube.vert",
                                   basePath + "light_cube.frag");
    }

    entt::registry m_registry;

    ShaderManager m_shaderManager;
    TextureManager m_textureManager;
    MeshFactory m_meshFactory;

    CameraSystem m_cameraSystem;
    LightSystem m_lightSystem;
    RenderSystem m_renderSystem{&m_shaderManager};
};

#endif // ENGINE_ENGINE_H
