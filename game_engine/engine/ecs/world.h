#ifndef GAME_ENGINE_ECS_WORLD_H
#define GAME_ENGINE_ECS_WORLD_H

#include <flecs_no_addons.h>
#include "components.h"

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// ============================================================
// Flecs 组件 ID 声明 — inline 避免多编译单元重复定义
// ECS_COMPONENT_DEFINE 在构造函数中注册到 Flecs 世界
// ============================================================
inline ecs_entity_t ecs_id(TransformComponent);
inline ecs_entity_t ecs_id(MeshComponent);
inline ecs_entity_t ecs_id(MaterialComponent);
inline ecs_entity_t ecs_id(LightComponent);
inline ecs_entity_t ecs_id(CameraComponent);
inline ecs_entity_t ecs_id(TagComponent);

// ============================================================
// EngineWorld — Flecs 世界封装
// 职责：实体 CRUD + 场景 JSON 序列化 / 反序列化
// ============================================================
class EngineWorld
{
public:
    EngineWorld()
    {
        m_world = ecs_init();
        ECS_COMPONENT_DEFINE(m_world, TransformComponent);
        ECS_COMPONENT_DEFINE(m_world, MeshComponent);
        ECS_COMPONENT_DEFINE(m_world, MaterialComponent);
        ECS_COMPONENT_DEFINE(m_world, LightComponent);
        ECS_COMPONENT_DEFINE(m_world, CameraComponent);
        ECS_COMPONENT_DEFINE(m_world, TagComponent);
    }

    ~EngineWorld()
    {
        if (m_world)
            ecs_fini(m_world);
    }

    ecs_world_t *raw() const { return m_world; }

    // ---- 实体操作 ----
    ecs_entity_t createEntity(const std::string &name = "Entity")
    {
        ecs_entity_t e = ecs_new(m_world);
        ecs_set_name(m_world, e, name.c_str());

        TagComponent t{};
        strncpy(t.name, name.c_str(), 63);
        t.name[63] = '\0';
        ecs_set_ptr(m_world, e, TagComponent, &t);
        return e;
    }

    void destroyEntity(ecs_entity_t e) { ecs_delete(m_world, e); }

    // 获取所有带 TagComponent 的实体
    std::vector<ecs_entity_t> getAllEntities()
    {
        std::vector<ecs_entity_t> result;
        ecs_query_desc_t qd = {};
        qd.terms[0].id = ecs_id(TagComponent);
        ecs_query_t *q = ecs_query_init(m_world, &qd);
        ecs_iter_t it = ecs_query_iter(m_world, q);
        while (ecs_query_next(&it))
            for (int i = 0; i < it.count; ++i)
                result.push_back(it.entities[i]);
        ecs_query_fini(q);
        return result;
    }

    // ---- JSON 序列化（nlohmann::json） ----
    std::string exportToJson()
    {
        nlohmann::json arr = nlohmann::json::array();

        for (auto e : getAllEntities())
        {
            const auto *tag = static_cast<const TagComponent *>(ecs_get(m_world, e, TagComponent));
            if (!tag)
                continue;

            nlohmann::json obj;
            obj["name"] = tag->name;
            obj["id"] = static_cast<uint64_t>(e);

            if (const auto *t = static_cast<const TransformComponent *>(ecs_get(m_world, e, TransformComponent)))
            {
                obj["transform"] = {
                    {"px", t->position.x}, {"py", t->position.y}, {"pz", t->position.z}, {"rx", t->rotation.x}, {"ry", t->rotation.y}, {"rz", t->rotation.z}, {"sx", t->scale.x}, {"sy", t->scale.y}, {"sz", t->scale.z}};
            }

            if (const auto *l = static_cast<const LightComponent *>(ecs_get(m_world, e, LightComponent)))
            {
                obj["light"] = {
                    {"type", static_cast<int>(l->type)},
                    {"ar", l->ambient.x},
                    {"ag", l->ambient.y},
                    {"ab", l->ambient.z},
                    {"dr", l->diffuse.x},
                    {"dg", l->diffuse.y},
                    {"db", l->diffuse.z}};
            }

            if (const auto *c = static_cast<const CameraComponent *>(ecs_get(m_world, e, CameraComponent)))
            {
                obj["camera"] = {
                    {"fov", c->fov}, {"near", c->nearPlane}, {"far", c->farPlane}, {"active", c->active}};
            }

            arr.push_back(obj);
        }

        return arr.dump(2); // 缩进 2 空格
    }

    bool importFromJson(const std::string &json)
    {
        auto doc = nlohmann::json::parse(json, nullptr, false);
        if (doc.is_discarded() || !doc.is_array())
            return false;

        for (auto &obj : doc)
        {
            auto e = createEntity(obj.value("name", "Entity"));

            if (obj.contains("transform"))
            {
                auto &t = obj["transform"];
                TransformComponent tc{};
                tc.position = {t["px"].get<float>(), t["py"].get<float>(), t["pz"].get<float>()};
                tc.rotation = {t["rx"].get<float>(), t["ry"].get<float>(), t["rz"].get<float>()};
                tc.scale = {t["sx"].get<float>(), t["sy"].get<float>(), t["sz"].get<float>()};
                ecs_set_ptr(m_world, e, TransformComponent, &tc);
            }

            if (obj.contains("light"))
            {
                auto &l = obj["light"];
                LightComponent lc{};
                lc.type = static_cast<LightType>(l["type"].get<int>());
                lc.ambient = {l["ar"].get<float>(), l["ag"].get<float>(), l["ab"].get<float>()};
                lc.diffuse = {l["dr"].get<float>(), l["dg"].get<float>(), l["db"].get<float>()};
                ecs_set_ptr(m_world, e, LightComponent, &lc);
            }

            if (obj.contains("camera"))
            {
                auto &c = obj["camera"];
                CameraComponent cc{};
                cc.fov = c["fov"].get<float>();
                cc.nearPlane = c["near"].get<float>();
                cc.farPlane = c["far"].get<float>();
                cc.active = c.value("active", true);
                ecs_set_ptr(m_world, e, CameraComponent, &cc);
            }
        }
        return true;
    }

private:
    ecs_world_t *m_world = nullptr;
};

#endif // GAME_ENGINE_ECS_WORLD_H
