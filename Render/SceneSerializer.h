#ifndef SCENESERIALIZER_H
#define SCENESERIALIZER_H

// ============================================================
// ⚠ 场景文件（.json/.prefab）禁止手写！（坑 15，实踩）
//
// flecs 反射的 JSON 格式是嵌套数组包裹（"x":[0.5] 而非 0.5），
// 手写 [x,y,z] 或 {"x":..} 都会解析失败且【静默】跳过——
// 所有字段变默认值 0，实体全挤原点，症状诡异难查。
// 正确做法：编辑器 💾 保存 / SceneSerializer::save /
// test_gameplay 程序化建场景后 save。
// 详见 docs/troubleshooting.md 坑 15。
// ============================================================

#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/CameraComponent.h"
#include "Components/TagComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/ScriptComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AudioComponent.h"
#include "Render/ResourceManager.h"

using json = nlohmann::json;

/// 辅助：用 C API 读取组件指针（避免 Flecs C++ e.get<>() 在 lambda 中的模板推导问题）
template <typename T>
inline const T *getComponent(flecs::world *w, flecs::entity_t id)
{
    return static_cast<const T *>(ecs_get_id(w->c_ptr(), id, w->component<T>().raw_id()));
}

class SceneSerializer
{
public:
    // ============================================================
    // 单实体序列化（save/load 和 Undo/Redo 命令快照共用）
    //
    // 【反射自动化】不再手写每个组件每个字段！
    //   自动路径：凡在 Engine::registerReflection 注册过 member 的组件
    //           （带 EcsStruct 元数据）→ flecs JSON addon 自动转换
    //           → 加新组件只需注册反射，序列化零改动（单一事实来源）
    //   手工路径：含运行时资源/非 POD 成员的组件（未注册反射 → 自动路径天然跳过）：
    //           TagComponent(char数组) / MeshComponent(句柄+acquire) / ScriptComponent(std::map)
    // ============================================================

    /// 实体的全部组件 → JSON（不含父子关系，由调用方处理）
    static json componentsToJson(flecs::world &world, flecs::entity_t id)
    {
        flecs::world *w = &world;
        json ent;

        // ---- 自动路径：遍历实体所有组件，带反射元数据的直接转 JSON ----
        const ecs_type_t *type = ecs_get_type(world.c_ptr(), id);
        if (type)
        {
            for (int i = 0; i < type->count; ++i)
            {
                ecs_id_t cid = type->array[i];
                if (ECS_IS_PAIR(cid))
                    continue; // 跳过关系对（ChildOf 等，父子关系另存）
                // 只处理注册过反射的组件（EcsStruct = 有字段清单）
                if (!ecs_has_id(world.c_ptr(), cid, ecs_id(EcsStruct)))
                    continue;
                const char *name = ecs_get_name(world.c_ptr(), cid);
                const void *ptr = ecs_get_id(world.c_ptr(), id, cid);
                if (!name || !ptr)
                    continue;
                // flecs JSON addon：按反射元数据自动序列化任意组件
                char *str = ecs_ptr_to_json(world.c_ptr(), cid, ptr);
                if (str)
                {
                    ent[name] = json::parse(str, nullptr, false);
                    ecs_os_free(str);
                }
            }
        }

        // ---- 手工路径：未注册反射的特殊组件 ----
        if (const TagComponent *tag = getComponent<TagComponent>(w, id))
            ent["TagComponent"]["name"] = tag->name;

        if (const MeshComponent *m = getComponent<MeshComponent>(w, id))
            ent["MeshComponent"]["meshType"] = m->meshType; // 句柄不存盘，存资源名

        if (const MaterialComponent *m = getComponent<MaterialComponent>(w, id))
            if (m->albedoTex[0])
                ent["MaterialComponent"]["albedoTex"] = m->albedoTex;

        if (const ScriptComponent *s = getComponent<ScriptComponent>(w, id))
        {
            if (s->filePath[0])
                ent["ScriptComponent"]["filePath"] = s->filePath;
            for (const auto &kv : s->properties)
                ent["ScriptComponent"]["properties"][kv.first] = kv.second;
        }

        // AnimationComponent：float/bool 字段走自动反射，char[] 字段手工补充
        if (const AnimationComponent *a = getComponent<AnimationComponent>(w, id))
        {
            ent["AnimationComponent"]["animFile"] = a->animFile;
            ent["AnimationComponent"]["current"] = a->current;
        }

        // 精灵帧动画（folder 是 char 数组不在反射里，手工序列化；
        //  spriteHandle 是运行时句柄不存，加载后渲染时惰性重建）
        if (const SpriteComponent *sp = getComponent<SpriteComponent>(w, id))
        {
            ent["SpriteComponent"]["folder"] = sp->folder;
            ent["SpriteComponent"]["frame"] = sp->frame;
        }

        // 音频（file 是 char 数组不在反射里，手工序列化；soundHandle 运行时重建）
        if (const AudioComponent *au = getComponent<AudioComponent>(w, id))
        {
            ent["AudioComponent"]["file"] = au->file;
            ent["AudioComponent"]["volume"] = au->volume;
            ent["AudioComponent"]["loop"] = au->loop;
            ent["AudioComponent"]["autoplay"] = au->autoplay;
            ent["AudioComponent"]["spatial"] = au->spatial;
        }
        return ent;
    }

    /// JSON → 写入实体的全部组件（Mesh 会 acquire 资源引用，需 GL 上下文）
    static void componentsFromJson(flecs::entity e, const json &ent)
    {
        ecs_world_t *w = e.world().c_ptr();

        for (auto it = ent.begin(); it != ent.end(); ++it)
        {
            const std::string &key = it.key();

            // 元数据字段（父子关系由调用方处理）
            if (key == "parent" || key == "__parentIdx")
                continue;

            // ---- 手工路径：特殊组件 ----
            if (key == "TagComponent")
            {
                TagComponent tc{};
                std::string name = it.value()["name"].get<std::string>();
                strncpy(tc.name, name.c_str(), sizeof(tc.name) - 1);
                e.set<TagComponent>(tc);
                continue;
            }
            if (key == "MeshComponent")
            {
                MeshComponent mesh{};
                std::string type = it.value().value("meshType", "cube");
                // 句柄化：同名网格命中缓存，多实体共享 GPU 数据
                mesh.meshHandle = ResourceManager::instance().acquireMesh(type);
                strncpy(mesh.meshType, type.c_str(), sizeof(mesh.meshType) - 1);
                e.set<MeshComponent>(mesh);
                continue;
            }
            if (key == "ScriptComponent")
            {
                ScriptComponent sc{};
                std::string fp = it.value().value("filePath", "");
                strncpy(sc.filePath, fp.c_str(), sizeof(sc.filePath) - 1);
                if (it.value().contains("properties"))
                    for (auto &[k, v] : it.value()["properties"].items())
                        sc.properties[k] = v.get<float>();
                e.set<ScriptComponent>(sc);
                continue;
            }
            if (key == "AnimationComponent")
            {
                AnimationComponent ac{};
                std::string af = it.value().value("animFile", "");
                std::string cur = it.value().value("current", "");
                strncpy(ac.animFile, af.c_str(), sizeof(ac.animFile) - 1);
                strncpy(ac.current, cur.c_str(), sizeof(ac.current) - 1);
                ac.playing = it.value().value("playing", false);
                ac.autoplay = it.value().value("autoplay", true);
                ac.loop = it.value().value("loop", true);
                ac.speed = it.value().value("speed", 1.0f);
                ac.time = it.value().value("time", 0.0f);
                ac.prevTime = it.value().value("prevTime", 0.0f);
                ac.blendTime = it.value().value("blendTime", 0.0f);
                ac.blendDuration = it.value().value("blendDuration", 0.0f);
                e.set<AnimationComponent>(ac);
                continue;
            }
            if (key == "SpriteComponent")
            {
                SpriteComponent sp{};
                std::string folder = it.value().value("folder", "");
                strncpy(sp.folder, folder.c_str(), sizeof(sp.folder) - 1);
                sp.frame = it.value().value("frame", 0);
                sp.spriteHandle = 0; // 运行时句柄：渲染时惰性 acquire
                e.set<SpriteComponent>(sp);
                continue;
            }
            if (key == "AudioComponent")
            {
                AudioComponent au{};
                std::string file = it.value().value("file", "");
                strncpy(au.file, file.c_str(), sizeof(au.file) - 1);
                au.volume = it.value().value("volume", 1.0f);
                au.loop = it.value().value("loop", false);
                au.autoplay = it.value().value("autoplay", true);
                au.spatial = it.value().value("spatial", false);
                au.soundHandle = 0; // 运行时句柄：AudioSystem 惰性创建
                e.set<AudioComponent>(au);
                continue;
            }

            // ---- 自动路径：按组件名 lookup + 反射反序列化 ----
            ecs_entity_t cid = ecs_lookup(w, key.c_str());
            if (!cid || !ecs_has_id(w, cid, ecs_id(EcsStruct)))
                continue; // 未知组件（可能是新版本存档）→ 安全跳过

            ecs_add_id(w, e.raw_id(), cid); // 确保组件存在（默认构造）
            void *ptr = ecs_get_mut_id(w, e.raw_id(), cid);
            if (!ptr)
                continue;
            json reflected = it.value();
            std::string albedoTex;
            if (key == "MaterialComponent")
            {
                albedoTex = reflected.value("albedoTex", "");
                reflected.erase("albedoTex"); // char[] 不是 Flecs 反射字段
            }
            std::string s = reflected.dump();
            // flecs JSON addon：按反射元数据自动反序列化
            const char *end = ecs_ptr_from_json(w, cid, ptr, s.c_str(), nullptr);
            if (!end)
                throw std::runtime_error("invalid reflected component JSON: " + key);
            while (*end && std::isspace(static_cast<unsigned char>(*end)))
                ++end;
            if (*end)
                throw std::runtime_error("trailing data in reflected component JSON: " + key);
            if (key == "MaterialComponent")
            {
                auto *material = static_cast<MaterialComponent *>(ptr);
                strncpy(material->albedoTex, albedoTex.c_str(), sizeof(material->albedoTex) - 1);
                material->albedoTex[sizeof(material->albedoTex) - 1] = '\0';
                material->texHandle = 0;
            }
            ecs_modified_id(w, e.raw_id(), cid);
        }
    }

    // ============================================================
    // 子树快照/重建（Undo 命令和 Prefab 共用的底层工具）
    // ============================================================

    /// 递归快照整棵子树：arr[0] 是根，每项记录 __parentIdx（数组内下标）
    static void snapshotSubtree(flecs::world &w, flecs::entity e, json &arr, int parentIdx)
    {
        int myIdx = (int)arr.size();
        json j = componentsToJson(w, e.raw_id());
        j["__parentIdx"] = parentIdx;
        arr.push_back(j);
        e.children([&](flecs::entity c)
                   { snapshotSubtree(w, c, arr, myIdx); });
    }

    /// 从快照重建子树（含 Mesh acquire），根挂回 rootParent（0=根层级）。返回根实体
    static flecs::entity rebuildSubtree(flecs::world &w, const json &arr, flecs::entity_t rootParent)
    {
        std::vector<flecs::entity> created;
        for (auto &ent : arr)
        {
            auto e = w.entity();
            componentsFromJson(e, ent); // Mesh 在此 acquire
            created.push_back(e);
        }
        // 恢复子树内部父子关系
        for (size_t i = 0; i < created.size(); ++i)
        {
            int p = arr[i].value("__parentIdx", -1);
            if (p >= 0)
                created[i].child_of(created[p]);
        }
        // 根实体挂回原父（若原父还活着）
        if (!created.empty() && rootParent && w.is_alive(rootParent))
            created[0].child_of(w.entity(rootParent));
        return created.empty() ? flecs::entity::null() : created[0];
    }

    // ============================================================
    // Prefab（预制体）—— 原型模式，对标 Godot 场景实例化/Unity Prefab
    //
    // Prefab 文件 = 实体子树的 JSON 快照（复用 snapshotSubtree）
    //   做好一个"敌人"模板 → 存 enemy.prefab → 场景里实例化 N 个
    //   改模板重新保存 → 之后实例化的都是新版
    // ============================================================

    /// 把实体（含整棵子树）保存为预制体文件
    static bool savePrefab(flecs::world &world, flecs::entity root, const std::string &path)
    {
        if (!root || !root.is_alive())
            return false;
        json arr = json::array();
        snapshotSubtree(world, root, arr, -1);
        json doc;
        doc["prefab"] = arr; // 包一层，未来可加版本号/元信息
        return writeJsonAtomically(path, doc);
    }

    /// 从预制体文件实例化实体子树（需 GL 上下文：Mesh acquire）。返回根实体
    static flecs::entity instantiatePrefab(flecs::world &world, const std::string &path)
    {
        std::ifstream in(path);
        if (!in)
            return flecs::entity::null();
        json doc = json::parse(in, nullptr, false);
        if (doc.is_discarded() || !doc.contains("prefab"))
            return flecs::entity::null();
        return rebuildSubtree(world, doc["prefab"], 0);
    }

    static bool save(flecs::world &world, const std::string &path)
    {
        json root;
        root["entities"] = json::array();

        // 第一遍：建立实体 ID → 数组下标映射（父子关系用下标引用）
        std::map<flecs::entity_t, int> idToIndex;
        int index = 0;
        world.each([&](flecs::entity e, const TagComponent &)
                   {
            const CameraComponent *cam = getComponent<CameraComponent>(&world, e.raw_id());
            if (cam && cam->isEditor) return;
            idToIndex[e.raw_id()] = index++; });

        world.each([&](flecs::entity e, const TagComponent &)
                   {
            const CameraComponent *cam = getComponent<CameraComponent>(&world, e.raw_id());
            if (cam && cam->isEditor) return;

            // 全部组件 → JSON（公共函数，与 Undo 快照共用）
            json ent = componentsToJson(world, e.raw_id());

            // 父子关系：存父实体在数组中的下标（缺省 = 无父）
            flecs::entity parent = e.parent();
            if (parent && idToIndex.count(parent.raw_id()))
                ent["parent"] = idToIndex[parent.raw_id()];

            root["entities"].push_back(ent); });

        return writeJsonAtomically(path, root);
    }

    static bool load(flecs::world &world, const std::string &path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        // 非抛异常版 parse（项目错误码风格）：坏文件/截断 → 返回 false，
        // 不闪退。与 instantiatePrefab 一致（此处曾漏改，审查 P3）
        json root = json::parse(in, nullptr, false);
        if (root.is_discarded() || !root.is_object() ||
            !root.contains("entities") || !root["entities"].is_array())
            return false;

        const json &entities = root["entities"];
        if (entities.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        std::vector<int> parents(entities.size(), -1);
        for (size_t i = 0; i < entities.size(); ++i)
        {
            const json &ent = entities[i];
            if (!ent.is_object())
                return false;
            if (!ent.contains("parent"))
                continue;
            if (!ent["parent"].is_number_integer())
                return false;
            int64_t parent = -1;
            try
            {
                parent = ent["parent"].get<int64_t>();
            }
            catch (...)
            {
                return false;
            }
            if (parent < 0 || parent >= static_cast<int64_t>(entities.size()) ||
                parent == static_cast<int64_t>(i))
                return false;
            parents[i] = static_cast<int>(parent);
        }

        // Reject cycles before creating entities or changing the live scene.
        std::vector<uint8_t> visit(entities.size(), 0);
        for (size_t i = 0; i < entities.size(); ++i)
        {
            std::vector<size_t> path;
            int current = static_cast<int>(i);
            while (current >= 0 && visit[static_cast<size_t>(current)] == 0)
            {
                visit[static_cast<size_t>(current)] = 1;
                path.push_back(static_cast<size_t>(current));
                current = parents[static_cast<size_t>(current)];
            }
            if (current >= 0 && visit[static_cast<size_t>(current)] == 1)
                return false;
            for (size_t idx : path)
                visit[idx] = 2;
        }

        // Collect the old scene first. New entities are staged alongside it so
        // a component parse failure cannot erase the currently loaded scene.
        std::vector<flecs::entity_t> toRemove;
        world.each([&](flecs::entity e, const TagComponent &)
                   {
            const CameraComponent *cam = getComponent<CameraComponent>(&world, e.raw_id());
            if (cam && cam->isEditor) return;
            toRemove.push_back(e.raw_id()); });

        std::vector<flecs::entity> created;
        created.reserve(entities.size());
        try
        {
            for (const auto &ent : entities)
            {
                auto e = world.entity();
                created.push_back(e);
                componentsFromJson(e, ent);
            }

            for (size_t i = 0; i < created.size(); ++i)
                if (parents[i] >= 0)
                    created[i].child_of(created[static_cast<size_t>(parents[i])]);
        }
        catch (...)
        {
            for (auto &e : created)
                if (e.is_alive())
                    e.destruct();
            return false;
        }

        // The staged scene is valid. Remove the previous non-editor scene now.
        for (auto id : toRemove)
            if (world.is_alive(id))
                world.entity(id).destruct();

        return true;
    }

private:
    static bool writeJsonAtomically(const std::string &path, const json &document)
    {
        try
        {
            const QFileInfo info(QString::fromStdString(path));
            if (!QDir().mkpath(info.absolutePath()))
                return false;

            const std::string serialized = document.dump(2);
            if (serialized.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
                return false;
            const QByteArray bytes(serialized.data(), static_cast<int>(serialized.size()));
            QSaveFile out(info.absoluteFilePath());
            if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size())
                return false;
            return out.commit();
        }
        catch (...)
        {
            return false;
        }
    }
};

#endif
