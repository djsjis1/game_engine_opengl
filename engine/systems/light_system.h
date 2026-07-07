#ifndef ENGINE_SYSTEMS_LIGHT_SYSTEM_H
#define ENGINE_SYSTEMS_LIGHT_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/light.h"
#include "../components/transform.h"
#include "../resources/shader.h"
#include <QVector3D>
#include <QString>
#include <QMatrix4x4>
#include <vector>

// ============================================================
// LightSystem: 光源收集与 Uniform 写入
//
// 遍历 registry 收集 LightComponent，渲染前通过 applyToShader
// 将光源数据写入当前 shader。最多 ENGINE_MAX_LIGHTS 个光源。
// ============================================================

#define ENGINE_MAX_LIGHTS 8

struct LightData
{
    int type = 0;
    QVector3D position = {0, 0, 0};
    QVector3D direction = {0, 0, -1};
    QVector3D ambient = {0.2f, 0.2f, 0.2f};
    QVector3D diffuse = {0.5f, 0.5f, 0.5f};
    QVector3D specular = {1, 1, 1};
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    float cutOff = 0.91f;
    float outerCutOff = 0.82f;
};

class LightSystem
{
public:
    void update(entt::registry &registry)
    {
        m_lightData.clear();

        auto view = registry.view<LightComponent>();
        for (auto entity : view)
        {
            if (m_lightData.size() >= ENGINE_MAX_LIGHTS)
                break;

            auto &light = view.get<LightComponent>(entity);
            LightData data;
            data.type = static_cast<int>(light.type);
            data.ambient = light.ambient;
            data.diffuse = light.diffuse;
            data.specular = light.specular;
            data.constant = light.constant;
            data.linear = light.linear;
            data.quadratic = light.quadratic;
            data.cutOff = light.cutOff;
            data.outerCutOff = light.outerCutOff;

            if (auto *t = registry.try_get<TransformComponent>(entity))
            {
                data.position = t->position;

                QMatrix4x4 rotMat;
                rotMat.rotate(t->rotation.x(), 1, 0, 0);
                rotMat.rotate(t->rotation.y(), 0, 1, 0);
                rotMat.rotate(t->rotation.z(), 0, 0, 1);
                QVector3D defaultDir(0, 0, -1);
                data.direction = (rotMat * defaultDir).normalized();
            }

            m_lightData.push_back(data);
        }
    }

    void applyToShader(Shader *shader)
    {
        if (!shader)
            return;

        shader->setInt("uLightCount", static_cast<int>(m_lightData.size()));

        for (size_t i = 0; i < m_lightData.size(); ++i)
        {
            const auto &l = m_lightData[i];
            QString prefix = QString("uLights[%1]").arg(i);

            shader->setInt(prefix + ".type", l.type);
            shader->setVec3(prefix + ".position", l.position.x(), l.position.y(), l.position.z());
            shader->setVec3(prefix + ".direction", l.direction.x(), l.direction.y(), l.direction.z());
            shader->setVec3(prefix + ".ambient", l.ambient.x(), l.ambient.y(), l.ambient.z());
            shader->setVec3(prefix + ".diffuse", l.diffuse.x(), l.diffuse.y(), l.diffuse.z());
            shader->setVec3(prefix + ".specular", l.specular.x(), l.specular.y(), l.specular.z());
            shader->setFloat(prefix + ".constant", l.constant);
            shader->setFloat(prefix + ".linear", l.linear);
            shader->setFloat(prefix + ".quadratic", l.quadratic);
            shader->setFloat(prefix + ".cutOff", l.cutOff);
            shader->setFloat(prefix + ".outerCutOff", l.outerCutOff);
        }
    }

    int getLightCount() const { return static_cast<int>(m_lightData.size()); }

private:
    std::vector<LightData> m_lightData;
};

#endif // ENGINE_SYSTEMS_LIGHT_SYSTEM_H
