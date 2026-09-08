#ifndef EDITORCOMMANDS_H
#define EDITORCOMMANDS_H

#include "Core/CommandStack.h"
#include "Render/SceneSerializer.h" // componentsToJson/FromJson（快照复用）
#include "Systems/PhysicsSystem.h"  // MoveEntityCommand 同步物理体
#include <cstring>

// ============================================================
// EditorCommands — 编辑器命令（每种操作一对 execute/undo）
//
// 【实体快照策略】
//   删除/重建用 JSON 快照（复用 SceneSerializer 的单实体序列化）：
//   - DeleteEntityCommand::execute 前快照【整棵子树】（删父会级联删子）
//   - undo 时从快照重建子树 + 恢复父子关系 + Mesh 资源重新 acquire
//
// 【已知局限（工业引擎的解法是稳定 GUID）】
//   实体重建后 ID 会变。命令自身会更新所记录的 ID（保证自己可重做），
//   但更早的命令若引用同一实体的旧 ID，撤销时会因 is_alive 检查而安全跳过
//   （不崩溃，只是那条历史失效）。
//
// 【GL 上下文】ResourceManager 延迟化后，acquire/release 都是纯记账，
// 命令在任何时机执行都安全，不再需要 makeCurrent 包裹。
// ============================================================

// 子树快照/重建工具在 SceneSerializer（序列化层）：
//   SceneSerializer::snapshotSubtree / rebuildSubtree
// Mesh 资源释放由 Engine 的 OnRemove Observer 自动完成，命令无需关心

// ============================================================
// 【Gizmo 变换】视口拖动实体的撤销（移动/旋转/缩放三模式统一）。
// mode: 0=position 1=rotation 2=scale。拖前记旧值，释放时入栈。
// position 模式额外同步物理体（同 set_position 双通道）。
// ============================================================
class TransformEditCommand : public Command
{
public:
    TransformEditCommand(flecs::world *w, flecs::entity_t id, int mode,
                         const glm::vec3 &oldV, const glm::vec3 &newV)
        : m_world(w), m_id(id), m_mode(mode), m_old(oldV), m_new(newV) {}

    void execute() override { apply(m_new); }
    void undo() override { apply(m_old); }
    const char *name() const override
    {
        return m_mode == 1 ? "旋转实体" : m_mode == 2 ? "缩放实体"
                                                      : "移动实体";
    }

private:
    void apply(const glm::vec3 &v)
    {
        if (!m_world->is_alive(m_id))
            return; // 实体已删：历史安全失效
        auto *tf = (TransformComponent *)ecs_get_mut_id(
            m_world->c_ptr(), m_id,
            m_world->component<TransformComponent>().raw_id());
        if (!tf)
            return;
        if (m_mode == 1)
            tf->rotation = v;
        else if (m_mode == 2)
            tf->scale = v;
        else
        {
            tf->position = v;
            physicsSetPosition(m_id, v.x, v.y, v.z); // 有刚体的一起搬
        }
    }

    flecs::world *m_world;
    flecs::entity_t m_id;
    int m_mode;
    glm::vec3 m_old, m_new;
};

// ============================================================
// 【新建实体】undo=删除；redo=从快照重建（保持组件/属性一致）
// ============================================================
class CreateEntityCommand : public Command
{
public:
    CreateEntityCommand(flecs::world *w, const std::string &entityName,
                        flecs::entity_t parent = 0)
        : m_world(w), m_name(entityName), m_parent(parent) {}

    void execute() override
    {
        if (m_snapshot.empty())
        {
            // 首次执行：创建默认可见实体（Tag+Transform+Material+Mesh）
            auto e = m_world->entity();
            TagComponent tc{};
            strncpy(tc.name, m_name.c_str(), sizeof(tc.name) - 1);
            e.set<TagComponent>(tc);
            e.set<TransformComponent>({});
            e.set<MaterialComponent>({});
            MeshComponent m;
            m.meshHandle = ResourceManager::instance().acquireMesh("cube");
            e.set<MeshComponent>(m);
            // 指定了父实体则挂为子节点（对标 Godot “创建子节点”）
            if (m_parent && m_world->is_alive(m_parent))
                e.child_of(m_world->entity(m_parent));
            m_id = e.raw_id();
            // 立即快照，redo 时走重建路径
            m_snapshot = json::array();
            SceneSerializer::snapshotSubtree(*m_world, e, m_snapshot, -1);
        }
        else
        {
            // redo：从快照重建（撤销后实体已销毁）
            m_id = SceneSerializer::rebuildSubtree(*m_world, m_snapshot, 0).raw_id();
        }
    }

    void undo() override
    {
        if (!m_world->is_alive(m_id))
            return;
        // Mesh 资源由 OnRemove Observer 自动释放，无需手动处理
        m_world->entity(m_id).destruct();
    }

    const char *name() const override { return "新建实体"; }

private:
    flecs::world *m_world;
    std::string m_name;
    flecs::entity_t m_parent = 0; // 可选父实体（0=创建为根）
    flecs::entity_t m_id = 0;
    json m_snapshot; // 空 = 还没首次执行
};

// ============================================================
// 【删除实体】execute=快照子树+删除；undo=重建子树+恢复父子
// ============================================================
class DeleteEntityCommand : public Command
{
public:
    DeleteEntityCommand(flecs::world *w, flecs::entity_t id)
        : m_world(w), m_id(id) {}

    void execute() override
    {
        if (!m_world->is_alive(m_id))
            return;
        auto e = m_world->entity(m_id);
        // 快照整棵子树（删父会级联删子，undo 要全部还原）
        m_snapshot = json::array();
        SceneSerializer::snapshotSubtree(*m_world, e, m_snapshot, -1);
        // 记录原父（undo 时挂回去）
        flecs::entity p = e.parent();
        m_parentId = p ? p.raw_id() : 0;
        // 删除（Mesh 资源由 OnRemove Observer 自动释放，含级联删除的子实体）
        e.destruct();
    }

    void undo() override
    {
        // 重建后 ID 变了 → 更新记录，保证 redo 删的是新实体
        m_id = SceneSerializer::rebuildSubtree(*m_world, m_snapshot, m_parentId).raw_id();
    }

    const char *name() const override { return "删除实体"; }

private:
    flecs::world *m_world;
    flecs::entity_t m_id;
    flecs::entity_t m_parentId = 0;
    json m_snapshot;
};

// ============================================================
// 【实例化预制体】原型模式：从 .prefab 文件创建实体子树
// execute=首次读文件实例化（之后 redo 用内存快照，不依赖文件还在）
// undo=释放资源+删除
// ============================================================
class InstantiatePrefabCommand : public Command
{
public:
    InstantiatePrefabCommand(flecs::world *w, const std::string &path)
        : m_world(w), m_path(path) {}

    void execute() override
    {
        if (m_snapshot.empty())
        {
            // 首次：从文件实例化，并把快照留在内存（redo 不再读盘）
            flecs::entity e = SceneSerializer::instantiatePrefab(*m_world, m_path);
            if (!e)
                return; // 文件无效 → 空命令（undo/redo 都无操作）
            m_id = e.raw_id();
            m_snapshot = json::array();
            SceneSerializer::snapshotSubtree(*m_world, e, m_snapshot, -1);
        }
        else
        {
            m_id = SceneSerializer::rebuildSubtree(*m_world, m_snapshot, 0).raw_id();
        }
    }

    void undo() override
    {
        if (!m_id || !m_world->is_alive(m_id))
            return;
        // Mesh 资源由 OnRemove Observer 自动释放
        m_world->entity(m_id).destruct();
    }

    const char *name() const override { return "实例化预制体"; }

private:
    flecs::world *m_world;
    std::string m_path;
    flecs::entity_t m_id = 0;
    json m_snapshot;
};

// ============================================================
// 【设置父级】记录旧父/新父，undo 恢复旧父（0=根层级）
// ============================================================
class SetParentCommand : public Command
{
public:
    SetParentCommand(flecs::world *w, flecs::entity_t child,
                     flecs::entity_t oldParent, flecs::entity_t newParent)
        : m_world(w), m_child(child), m_oldParent(oldParent), m_newParent(newParent) {}

    void execute() override { apply(m_newParent); }
    void undo() override { apply(m_oldParent); }
    const char *name() const override { return "设置父级"; }

private:
    void apply(flecs::entity_t parentId)
    {
        if (!m_world->is_alive(m_child))
            return; // 实体已被别的操作删除 → 安全跳过
        auto c = m_world->entity(m_child);
        if (parentId && m_world->is_alive(parentId))
            c.child_of(m_world->entity(parentId));
        else
            c.remove(flecs::ChildOf, flecs::Wildcard); // 回到根层级
    }

    flecs::world *m_world;
    flecs::entity_t m_child, m_oldParent, m_newParent;
};

// ============================================================
// 【显示/隐藏】翻转操作：undo 和 execute 是同一动作
// ============================================================
class ToggleVisibleCommand : public Command
{
public:
    ToggleVisibleCommand(flecs::world *w, flecs::entity_t id)
        : m_world(w), m_id(id) {}

    void execute() override { toggle(); }
    void undo() override { toggle(); }
    const char *name() const override { return "显示/隐藏"; }

private:
    void toggle()
    {
        if (!m_world->is_alive(m_id))
            return;
        auto *tf = (TransformComponent *)ecs_get_mut_id(
            m_world->c_ptr(), m_id, m_world->component<TransformComponent>().raw_id());
        if (tf)
            tf->visible = !tf->visible;
    }

    flecs::world *m_world;
    flecs::entity_t m_id;
};

#endif
