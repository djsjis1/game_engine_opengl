#ifndef ENGINE_SYSTEMS_RENDER_SYSTEM_H
#define ENGINE_SYSTEMS_RENDER_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/transform.h"
#include "../components/mesh.h"
#include "../components/material.h"
#include "../resources/shader_manager.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <vector>
#include <functional>

// ============================================================
// RenderSystem: 主渲染系统
//
// 遍历具有 Transform + Mesh 的实体，按 shader 分组渲染。
//
// 渲染流程：
//   1. registry.view<TransformComponent, MeshComponent>()
//   2. 按 MaterialComponent::shaderName 排序
//   3. 每个 shader：设置 view/projection/cameraPos/光源 → 逐个实体画
// ============================================================
class RenderSystem : protected QOpenGLFunctions_3_3_Core
{
public:
    RenderSystem(ShaderManager *shaderMgr)
        : m_shaderManager(shaderMgr) {} // GL 函数在首次 render() 时懒初始化

    void render(entt::registry &registry)
    {
        if (!m_shaderManager)
            return;
        ensureInit(); // 懒初始化（首次渲染时上下文已就绪）

        struct RenderBatch
        {
            entt::entity entity;
            QString shaderName;
        };
        std::vector<RenderBatch> batches;

        auto view = registry.view<TransformComponent, MeshComponent>();
        for (auto entity : view)
        {
            QString shaderName = "default";
            if (auto *mat = registry.try_get<MaterialComponent>(entity))
                shaderName = mat->shaderName;

            batches.push_back({entity, shaderName});
        }

        std::sort(batches.begin(), batches.end(),
                  [](const RenderBatch &a, const RenderBatch &b)
                  {
                      return a.shaderName < b.shaderName;
                  });

        Shader *currentShader = nullptr;

        for (const auto &batch : batches)
        {
            Shader *shader = m_shaderManager->getShader(batch.shaderName);
            if (!shader)
                shader = m_shaderManager->getDefaultShader();
            if (!shader)
                continue;

            if (shader != currentShader)
            {
                currentShader = shader;
                shader->use();

                shader->setMat4("uView", m_viewMatrix.constData());
                shader->setMat4("uProjection", m_projectionMatrix.constData());
                shader->setVec3("uCameraPos",
                                m_cameraPosition.x(),
                                m_cameraPosition.y(),
                                m_cameraPosition.z());

                if (m_lightApplyFunc)
                    m_lightApplyFunc(shader);
            }

            auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(batch.entity);
            if (!mesh.isValid())
                continue;

            QMatrix4x4 model = transform.getModelMatrix();
            shader->setMat4("uModel", model.constData());

            QMatrix3x3 normalMat = transform.getNormalMatrix();
            float normalData[9];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    normalData[i * 3 + j] = normalMat(j, i);
            shader->setMat3("uNormalMatrix", normalData);

            if (auto *mat = registry.try_get<MaterialComponent>(batch.entity))
            {
                shader->setVec3("uMaterial.ambient",
                                mat->ambient.x(), mat->ambient.y(), mat->ambient.z());
                shader->setVec3("uMaterial.diffuse",
                                mat->diffuse.x(), mat->diffuse.y(), mat->diffuse.z());
                shader->setVec3("uMaterial.specular",
                                mat->specular.x(), mat->specular.y(), mat->specular.z());
                shader->setFloat("uMaterial.shininess", mat->shininess);

                if (mat->diffuseTexture != 0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mat->diffuseTexture);
                    shader->setInt("uMaterial.diffuseMap", 0);
                }
                if (mat->specularTexture != 0)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, mat->specularTexture);
                    shader->setInt("uMaterial.specularMap", 1);
                }
            }

            glBindVertexArray(mesh.vao);
            if (mesh.hasIndices && mesh.indexCount > 0)
                glDrawElements(mesh.primitiveType, mesh.indexCount, GL_UNSIGNED_INT, 0);
            else
                glDrawArrays(mesh.primitiveType, 0, mesh.vertexCount);
            glBindVertexArray(0);
        }
    }

    void setViewMatrix(const QMatrix4x4 &view) { m_viewMatrix = view; }
    void setProjectionMatrix(const QMatrix4x4 &proj) { m_projectionMatrix = proj; }
    void setCameraPosition(const QVector3D &pos) { m_cameraPosition = pos; }

    using LightApplyFunc = std::function<void(Shader *)>;
    void setLightApplyFunc(LightApplyFunc func) { m_lightApplyFunc = std::move(func); }

private:
    void ensureInit()
    {
        if (!m_glInitialized)
        {
            initializeOpenGLFunctions();
            m_glInitialized = true;
        }
    }

    ShaderManager *m_shaderManager = nullptr;
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_projectionMatrix;
    QVector3D m_cameraPosition;
    LightApplyFunc m_lightApplyFunc;
    bool m_glInitialized = false;
};

#endif // ENGINE_SYSTEMS_RENDER_SYSTEM_H
