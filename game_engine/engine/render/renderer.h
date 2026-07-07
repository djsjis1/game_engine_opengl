#ifndef GAME_ENGINE_RENDER_RENDERER_H
#define GAME_ENGINE_RENDER_RENDERER_H

#include <flecs_no_addons.h>
#include <QOpenGLFunctions_3_3_Core>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <algorithm>

#include "../ecs/components.h"
#include "shader.h"
#include "mesh_factory.h"

#define MAX_LIGHTS 8

// Flecs C API 组件 ID 辅助宏
#define GET_CID(world, T) ecs_id(T)

// ============================================================
// Renderer — 前向 Phong 多光源渲染管线
//
// 每帧流程：
//   1. beginFrame(world)   — 扫描所有 Light + Camera，计算 view/proj
//   2. renderEntities(world) — 按 shader 分组绘制所有 (Transform+Mesh)
//
// 光源数据在 beginFrame 中收集，renderEntities 中注入到 Shader uniform。
// ============================================================
class Renderer : protected QOpenGLFunctions_3_3_Core
{
public:
    Renderer() = default;

    void ensureInit()
    {
        if (!m_glInit)
        {
            initializeOpenGLFunctions();
            m_glInit = true;
        }
    }

    bool initShaders(const std::string &basePath)
    {
        return m_defaultShader.load(basePath + "default.vert", basePath + "default.frag") && m_lightCubeShader.load(basePath + "light_cube.vert", basePath + "light_cube.frag");
    }

    // ---- 帧开始：收集光源 + 计算摄像机矩阵 ----
    void beginFrame(ecs_world_t *world)
    {
        ensureInit();
        m_lights.clear();

        // 1. 收集光源数据
        ecs_query_desc_t qd = {};
        qd.terms[0].id = ecs_id(LightComponent);
        ecs_query_t *q = ecs_query_init(world, &qd);
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it))
        {
            auto *lights = ecs_field(&it, LightComponent, 1);
            for (int i = 0; i < it.count && static_cast<int>(m_lights.size()) < MAX_LIGHTS; ++i)
            {
                LightData d;
                d.type = static_cast<int>(lights[i].type);
                d.ambient = lights[i].ambient;
                d.diffuse = lights[i].diffuse;
                d.specular = lights[i].specular;
                d.constant = lights[i].constant;
                d.linear = lights[i].linear;
                d.quadratic = lights[i].quadratic;
                d.cutOff = lights[i].cutOff;
                d.outerCutOff = lights[i].outerCutOff;

                // 从 Transform 拿位置和方向
                auto *t = static_cast<const TransformComponent *>(
                    ecs_get(world, it.entities[i], TransformComponent));
                if (t)
                {
                    d.position = t->position;
                    glm::mat4 rot(1.0f);
                    rot = glm::rotate(rot, glm::radians(t->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                    rot = glm::rotate(rot, glm::radians(t->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                    rot = glm::rotate(rot, glm::radians(t->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
                    d.direction = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                }
                m_lights.push_back(d);
            }
        }
        ecs_query_fini(q);

        // 2. 收集摄像机数据（取第一个 active 的摄像机）
        ecs_query_desc_t cqd = {};
        cqd.terms[0].id = ecs_id(CameraComponent);
        ecs_query_t *cq = ecs_query_init(world, &cqd);
        ecs_iter_t cit = ecs_query_iter(world, cq);
        while (ecs_query_next(&cit))
        {
            auto *cameras = ecs_field(&cit, CameraComponent, 1);
            for (int i = 0; i < cit.count; ++i)
            {
                if (!cameras[i].active)
                    continue;

                glm::vec3 pos = cameras[i].position;
                glm::vec3 front = cameras[i].front;
                glm::vec3 up = cameras[i].up;

                // Transform 优先覆盖 position
                auto *t = static_cast<const TransformComponent *>(
                    ecs_get(world, cit.entities[i], TransformComponent));
                if (t)
                    pos = t->position;

                m_viewMatrix = glm::lookAt(pos, pos + front, up);
                m_projMatrix = glm::perspective(
                    glm::radians(cameras[i].fov), m_aspectRatio,
                    cameras[i].nearPlane, cameras[i].farPlane);
                m_cameraPos = pos;
                break;
            }
        }
        ecs_query_fini(cq);
    }

    // ---- 渲染所有实体 ----
    void renderEntities(ecs_world_t *world)
    {
        ensureInit();

        // 批次结构：按 shader 名分组，减少 glUseProgram 切换
        struct Batch
        {
            ecs_entity_t entity;
            std::string shaderName;
        };
        std::vector<Batch> batches;

        ecs_query_desc_t rqd = {};
        rqd.terms[0].id = ecs_id(TransformComponent);
        rqd.terms[1].id = ecs_id(MeshComponent);
        ecs_query_t *q = ecs_query_init(world, &rqd);
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it))
            for (int i = 0; i < it.count; ++i)
            {
                std::string sn = "default";
                auto *mat = static_cast<const MaterialComponent *>(
                    ecs_get(world, it.entities[i], MaterialComponent));
                if (mat && mat->shaderName[0])
                    sn = mat->shaderName;
                batches.push_back({it.entities[i], sn});
            }
        ecs_query_fini(q);

        // 按 shader 名排序（相同 shader 的实体连续绘制）
        std::sort(batches.begin(), batches.end(),
                  [](const Batch &a, const Batch &b)
                  { return a.shaderName < b.shaderName; });

        Shader *cur = nullptr;
        for (auto &b : batches)
        {
            Shader *s = getShader(b.shaderName);
            if (!s)
                continue;

            // 切换 Shader 时更新公共 uniform
            if (s != cur)
            {
                cur = s;
                s->use();
                s->setMat4("uView", glm::value_ptr(m_viewMatrix));
                s->setMat4("uProjection", glm::value_ptr(m_projMatrix));
                s->setVec3("uCameraPos", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
                applyLights(s);
            }

            auto *t = static_cast<const TransformComponent *>(ecs_get(world, b.entity, TransformComponent));
            auto *mesh = static_cast<const MeshComponent *>(ecs_get(world, b.entity, MeshComponent));
            if (!mesh || !mesh->isValid())
                continue;

            // 模型矩阵 + 法线矩阵
            glm::mat4 model = t ? t->getModelMatrix() : glm::mat4(1.0f);
            s->setMat4("uModel", glm::value_ptr(model));
            if (t)
            {
                glm::mat3 nm = t->getNormalMatrix();
                s->setMat3("uNormalMatrix", glm::value_ptr(nm));
            }

            // 材质属性
            auto *mat = static_cast<const MaterialComponent *>(ecs_get(world, b.entity, MaterialComponent));
            if (mat)
            {
                s->setVec3("uMaterial.ambient", mat->ambient.x, mat->ambient.y, mat->ambient.z);
                s->setVec3("uMaterial.diffuse", mat->diffuse.x, mat->diffuse.y, mat->diffuse.z);
                s->setVec3("uMaterial.specular", mat->specular.x, mat->specular.y, mat->specular.z);
                s->setFloat("uMaterial.shininess", mat->shininess);

                // 可选贴图
                if (mat->diffuseTexture)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mat->diffuseTexture);
                    s->setInt("uMaterial.diffuseMap", 0);
                }
                if (mat->specularTexture)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, mat->specularTexture);
                    s->setInt("uMaterial.specularMap", 1);
                }
            }

            // 绘制
            glBindVertexArray(mesh->vao);
            if (mesh->hasIndices && mesh->indexCount > 0)
                glDrawElements(mesh->primitiveType, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
            else
                glDrawArrays(mesh->primitiveType, 0, mesh->vertexCount);
            glBindVertexArray(0);
        }
    }

    void setAspectRatio(float r) { m_aspectRatio = r; }
    const glm::mat4 &viewMatrix() const { return m_viewMatrix; }
    MeshFactory &meshFactory() { return m_meshFactory; }

private:
    // CPU 端光源缓存（每帧从 ECS 世界同步）
    struct LightData
    {
        int type = 1;
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
        glm::vec3 ambient, diffuse, specular;
        float constant = 1.0f, linear = 0.09f, quadratic = 0.032f;
        float cutOff = 0.91f, outerCutOff = 0.82f;
    };

    Shader *getShader(const std::string &name)
    {
        return name == "light_cube" ? &m_lightCubeShader : &m_defaultShader;
    }

    // 将光源数组注入到当前 Shader 的 uLights[] uniform
    void applyLights(Shader *s)
    {
        s->setInt("uLightCount", static_cast<int>(m_lights.size()));
        for (size_t i = 0; i < m_lights.size(); ++i)
        {
            auto &l = m_lights[i];
            std::string p = "uLights[" + std::to_string(i) + "]";
            s->setInt(p + ".type", l.type);
            s->setVec3(p + ".position", l.position.x, l.position.y, l.position.z);
            s->setVec3(p + ".direction", l.direction.x, l.direction.y, l.direction.z);
            s->setVec3(p + ".ambient", l.ambient.x, l.ambient.y, l.ambient.z);
            s->setVec3(p + ".diffuse", l.diffuse.x, l.diffuse.y, l.diffuse.z);
            s->setVec3(p + ".specular", l.specular.x, l.specular.y, l.specular.z);
            s->setFloat(p + ".constant", l.constant);
            s->setFloat(p + ".linear", l.linear);
            s->setFloat(p + ".quadratic", l.quadratic);
            s->setFloat(p + ".cutOff", l.cutOff);
            s->setFloat(p + ".outerCutOff", l.outerCutOff);
        }
    }

    Shader m_defaultShader, m_lightCubeShader;
    MeshFactory m_meshFactory;
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::vec3 m_cameraPos{0.0f, 0.0f, 0.0f};
    float m_aspectRatio = 1.0f;
    bool m_glInit = false;
    std::vector<LightData> m_lights;
};

#endif // GAME_ENGINE_RENDER_RENDERER_H
#ifndef GAME_ENGINE_RENDER_RENDERER_H
#define GAME_ENGINE_RENDER_RENDERER_H

#include <flecs_no_addons.h>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QString>
#include <vector>
#include "../ecs/components.h"
#include "shader.h"
#include "mesh_factory.h"

#define MAX_LIGHTS 8

// Flecs C API 组件 ID 辅助宏
#define GET_CID(world, T) ecs_id(T)

class Renderer : protected QOpenGLFunctions_3_3_Core
{
public:
    Renderer() = default;
    void ensureInit()
    {
        if (!m_glInit)
        {
            initializeOpenGLFunctions();
            m_glInit = true;
        }
    }

    bool initShaders(const QString &basePath)
    {
        return m_defaultShader.load(basePath + "default.vert", basePath + "default.frag") && m_lightCubeShader.load(basePath + "light_cube.vert", basePath + "light_cube.frag");
    }

    void beginFrame(ecs_world_t *world)
    {
        ensureInit();
        m_lights.clear();

        ecs_query_desc_t qd = {};
        qd.terms[0].id = ecs_id(LightComponent);
        ecs_query_t *q = ecs_query_init(world, &qd);
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it))
        {
            auto *lights = ecs_field(&it, LightComponent, 1);
            for (int i = 0; i < it.count && (int)m_lights.size() < MAX_LIGHTS; ++i)
            {
                LightData d;
                d.type = static_cast<int>(lights[i].type);
                d.ambient = lights[i].ambient;
                d.diffuse = lights[i].diffuse;
                d.specular = lights[i].specular;
                d.constant = lights[i].constant;
                d.linear = lights[i].linear;
                d.quadratic = lights[i].quadratic;
                d.cutOff = lights[i].cutOff;
                d.outerCutOff = lights[i].outerCutOff;

                auto *t = static_cast<const TransformComponent *>(
                    ecs_get(world, it.entities[i], TransformComponent));
                if (t)
                {
                    d.position = t->position;
                    QMatrix4x4 rot;
                    rot.rotate(t->rotation.x(), 1, 0, 0);
                    rot.rotate(t->rotation.y(), 0, 1, 0);
                    rot.rotate(t->rotation.z(), 0, 0, 1);
                    d.direction = (rot * QVector3D(0, 0, -1)).normalized();
                }
                m_lights.push_back(d);
            }
        }
        ecs_query_fini(q);

        ecs_query_desc_t cqd = {};
        cqd.terms[0].id = ecs_id(CameraComponent);
        ecs_query_t *cq = ecs_query_init(world, &cqd);
        ecs_iter_t cit = ecs_query_iter(world, cq);
        while (ecs_query_next(&cit))
        {
            auto *cameras = ecs_field(&cit, CameraComponent, 1);
            for (int i = 0; i < cit.count; ++i)
            {
                if (!cameras[i].active)
                    continue;
                QVector3D pos = cameras[i].position;
                QVector3D front = cameras[i].front, up = cameras[i].up;
                auto *t = static_cast<const TransformComponent *>(
                    ecs_get(world, cit.entities[i], TransformComponent));
                if (t)
                    pos = t->position;
                m_viewMatrix.setToIdentity();
                m_viewMatrix.lookAt(pos, pos + front, up);
                m_projMatrix.setToIdentity();
                m_projMatrix.perspective(cameras[i].fov, m_aspectRatio,
                                         cameras[i].nearPlane, cameras[i].farPlane);
                m_cameraPos = pos;
                break;
            }
        }
        ecs_query_fini(cq);
    }

    void renderEntities(ecs_world_t *world)
    {
        ensureInit();
        struct Batch
        {
            ecs_entity_t entity;
            QString shaderName;
        };
        std::vector<Batch> batches;

        ecs_query_desc_t rqd = {};
        rqd.terms[0].id = ecs_id(TransformComponent);
        rqd.terms[1].id = ecs_id(MeshComponent);
        ecs_query_t *q = ecs_query_init(world, &rqd);
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it))
            for (int i = 0; i < it.count; ++i)
            {
                QString sn = "default";
                auto *mat = static_cast<const MaterialComponent *>(
                    ecs_get(world, it.entities[i], MaterialComponent));
                if (mat)
                    sn = QString::fromUtf8(mat->shaderName);
                batches.push_back({it.entities[i], sn});
            }
        ecs_query_fini(q);

        std::sort(batches.begin(), batches.end(),
                  [](const Batch &a, const Batch &b)
                  { return a.shaderName < b.shaderName; });

        Shader *cur = nullptr;
        for (auto &b : batches)
        {
            Shader *s = getShader(b.shaderName);
            if (!s)
                continue;
            if (s != cur)
            {
                cur = s;
                s->use();
                s->setMat4("uView", m_viewMatrix.constData());
                s->setMat4("uProjection", m_projMatrix.constData());
                s->setVec3("uCameraPos", m_cameraPos.x(), m_cameraPos.y(), m_cameraPos.z());
                applyLights(s);
            }
            auto *t = static_cast<const TransformComponent *>(ecs_get(world, b.entity, TransformComponent));
            auto *mesh = static_cast<const MeshComponent *>(ecs_get(world, b.entity, MeshComponent));
            if (!mesh || !mesh->isValid())
                continue;

            QMatrix4x4 model = t ? t->getModelMatrix() : QMatrix4x4();
            s->setMat4("uModel", model.constData());
            if (t)
            {
                QMatrix3x3 nm = t->getNormalMatrix();
                float nd[9];
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                        nd[c * 3 + r] = nm(r, c);
                s->setMat3("uNormalMatrix", nd);
            }

            auto *mat = static_cast<const MaterialComponent *>(ecs_get(world, b.entity, MaterialComponent));
            if (mat)
            {
                s->setVec3("uMaterial.ambient", mat->ambient.x(), mat->ambient.y(), mat->ambient.z());
                s->setVec3("uMaterial.diffuse", mat->diffuse.x(), mat->diffuse.y(), mat->diffuse.z());
                s->setVec3("uMaterial.specular", mat->specular.x(), mat->specular.y(), mat->specular.z());
                s->setFloat("uMaterial.shininess", mat->shininess);
                if (mat->diffuseTexture)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mat->diffuseTexture);
                    s->setInt("uMaterial.diffuseMap", 0);
                }
                if (mat->specularTexture)
                {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, mat->specularTexture);
                    s->setInt("uMaterial.specularMap", 1);
                }
            }
            glBindVertexArray(mesh->vao);
            if (mesh->hasIndices && mesh->indexCount > 0)
                glDrawElements(mesh->primitiveType, mesh->indexCount, GL_UNSIGNED_INT, 0);
            else
                glDrawArrays(mesh->primitiveType, 0, mesh->vertexCount);
            glBindVertexArray(0);
        }
    }

    void setAspectRatio(float r) { m_aspectRatio = r; }
    const QMatrix4x4 &viewMatrix() const { return m_viewMatrix; }
    MeshFactory &meshFactory() { return m_meshFactory; }

private:
    struct LightData
    {
        int type = 1;
        QVector3D position, direction{0, 0, -1}, ambient, diffuse, specular;
        float constant = 1, linear = 0.09f, quadratic = 0.032f, cutOff = 0.91f, outerCutOff = 0.82f;
    };
    Shader *getShader(const QString &name) { return name == "light_cube" ? &m_lightCubeShader : &m_defaultShader; }
    void applyLights(Shader *s)
    {
        s->setInt("uLightCount", (int)m_lights.size());
        for (size_t i = 0; i < m_lights.size(); ++i)
        {
            auto &l = m_lights[i];
            QString p = QString("uLights[%1]").arg(i);
            s->setInt(p + ".type", l.type);
            s->setVec3(p + ".position", l.position.x(), l.position.y(), l.position.z());
            s->setVec3(p + ".direction", l.direction.x(), l.direction.y(), l.direction.z());
            s->setVec3(p + ".ambient", l.ambient.x(), l.ambient.y(), l.ambient.z());
            s->setVec3(p + ".diffuse", l.diffuse.x(), l.diffuse.y(), l.diffuse.z());
            s->setVec3(p + ".specular", l.specular.x(), l.specular.y(), l.specular.z());
            s->setFloat(p + ".constant", l.constant);
            s->setFloat(p + ".linear", l.linear);
            s->setFloat(p + ".quadratic", l.quadratic);
            s->setFloat(p + ".cutOff", l.cutOff);
            s->setFloat(p + ".outerCutOff", l.outerCutOff);
        }
    }

    Shader m_defaultShader, m_lightCubeShader;
    MeshFactory m_meshFactory;
    QMatrix4x4 m_viewMatrix, m_projMatrix;
    QVector3D m_cameraPos;
    float m_aspectRatio = 1.0f;
    bool m_glInit = false;
    std::vector<LightData> m_lights;
};

#endif
