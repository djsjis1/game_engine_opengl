#ifndef SCENESNAPSHOT_H
#define SCENESNAPSHOT_H

#include <flecs.h>
#include <nlohmann/json.hpp>
#include "Render/SceneSerializer.h"

// ============================================================
// SceneSnapshot — 完整场景快照（对标 Godot 的 Play/Stop 场景隔离）
//
// 【Godot 的做法】按 F5 运行时复制整棵场景树，运行时操作副本，
//   停止后副本销毁，编辑器原件从头到尾没被动过。
//
// 【本模块的做法（等效）】
//   capture()：Play 前把整个场景序列化为 JSON 存内存（不写磁盘）
//   restore()：Stop 后清空世界，从内存 JSON 重新加载
//   效果等价于 Godot 的"副本销毁 + 原件不变"
//
// 【解耦设计】
//   本模块只依赖 SceneSerializer 的 save/load 接口。
//   如果将来要换快照方案（二进制格式 / 双世界 / 增量快照），
//   只改本文件，不动 Engine / MainWindow / 任何 System。
//
// 【对比旧方案】
//   旧：snapshotTransforms() 只存 Transform，材质/血量/新建实体全丢
//   新：全组件完整快照，Stop 后场景 100% 还原（含运行时新建的实体被清除）
// ============================================================

using json = nlohmann::json;

class SceneSnapshot
{
public:
    /// Play 前调用：完整序列化当前世界所有实体到内存
    void capture(flecs::world &world)
    {
        m_data = json::array();
        // 复用 SceneSerializer 的逐实体序列化（含父子关系）
        // 遍历所有有 TagComponent 的实体（即"有意义的实体"）
        world.each([&](flecs::entity e, TagComponent &)
                   {
            json ent = SceneSerializer::componentsToJson(world, e.raw_id());
            // 父子关系：有父则记录父实体名（加载时重建层级）
            flecs::entity parent = e.parent();
            if (parent && parent.is_alive())
            {
                const TagComponent *pt = static_cast<const TagComponent *>(
                    ecs_get_id(world.c_ptr(), parent.raw_id(),
                               world.component<TagComponent>().raw_id()));
                if (pt)
                    ent["__parentName"] = pt->name;
            }
            m_data.push_back(ent); });
        m_valid = true;
    }

    /// Stop 后调用：清空世界，从内存 JSON 完整重建
    void restore(flecs::world &world)
    {
        if (!m_valid)
            return;

        // ① 销毁所有实体（运行时新建的子弹/特效等一并清除）
        //    只销毁有 TagComponent 的实体（跳过 flecs 内置实体）
        //    收集后统一销毁（避免遍历中删除的迭代器失效）
        std::vector<flecs::entity_t> toDestroy;
        world.each([&](flecs::entity e, TagComponent &)
                   { toDestroy.push_back(e.raw_id()); });
        for (auto id : toDestroy)
        {
            flecs::entity e = world.entity(id);
            if (e.is_alive())
                e.destruct();
        }

        // ② 从快照重建所有实体
        //    先建实体 + 组件，再重建父子关系（两遍，避免父未建时挂子）
        struct PendingChild
        {
            std::string childName;
            std::string parentName;
        };
        std::vector<PendingChild> pendingChildren;

        for (auto &entJson : m_data)
        {
            auto e = world.entity();
            SceneSerializer::componentsFromJson(e, entJson);

            // 记录父子关系待第二遍处理
            if (entJson.contains("__parentName"))
            {
                std::string childName;
                if (entJson.contains("TagComponent") && entJson["TagComponent"].contains("name"))
                    childName = entJson["TagComponent"]["name"].get<std::string>();
                std::string parentName = entJson["__parentName"].get<std::string>();
                if (!childName.empty() && !parentName.empty())
                    pendingChildren.push_back({childName, parentName});
            }
        }

        // ③ 重建父子关系
        for (auto &pc : pendingChildren)
        {
            flecs::entity_t childId = 0, parentId = 0;
            world.each([&](flecs::entity e, const TagComponent &tag)
                       {
                if (std::string(tag.name) == pc.childName) childId = e.raw_id();
                if (std::string(tag.name) == pc.parentName) parentId = e.raw_id(); });
            if (childId && parentId)
                world.entity(childId).child_of(world.entity(parentId));
        }

        m_valid = false; // 快照用完即废（下次 Play 重新 capture）
    }

    /// 是否有有效快照（未 capture 或已 restore 后为 false）
    bool hasSnapshot() const { return m_valid; }

    /// 手动清除快照（释放内存）
    void clear()
    {
        m_data.clear();
        m_valid = false;
    }

private:
    json m_data;          // 完整场景 JSON（内存中，不写磁盘）
    bool m_valid = false; // 快照是否有效
};

#endif
