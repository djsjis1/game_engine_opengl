#ifndef COMMANDSTACK_H
#define COMMANDSTACK_H

#include <memory>
#include <vector>
#include <string>

// ============================================================
// CommandStack — 撤销/重做系统（命令模式，对标 Godot EditorUndoRedo）
//
// 【命令模式核心思想】
//   把"一次编辑操作"封装成对象，对象知道怎么做(execute)和怎么反悔(undo)：
//
//     用户操作 ──→ Command 对象 ──→ execute()
//                      │
//                      ▼ 压入撤销栈
//     Ctrl+Z ──→ 弹出 → undo()    → 压入重做栈
//     Ctrl+Y ──→ 弹出 → execute() → 压回撤销栈
//
// 【为什么执行新命令要清空重做栈】
//   撤销两步后做了新操作 → 历史分叉了，旧的"未来"不再成立
//   （所有编辑器都这样：Word/PS/Godot）
//
// 【内存控制】栈深超过 kMaxUndoDepth 时丢弃最老的命令
// ============================================================

/// 命令基类：每种编辑操作实现一对 execute/undo
class Command
{
public:
    virtual ~Command() = default;
    virtual void execute() = 0;           // 执行（含 redo 重做）
    virtual void undo() = 0;              // 撤销
    virtual const char *name() const = 0; // 命令名（调试/状态栏显示）
};

class CommandStack
{
public:
    static CommandStack &instance()
    {
        static CommandStack s;
        return s;
    }

    /// 执行命令并压入撤销栈（清空重做栈——历史分叉）
    void push(std::unique_ptr<Command> cmd)
    {
        cmd->execute();
        m_undoStack.push_back(std::move(cmd));
        m_redoStack.clear();
        // 超出上限 → 丢弃最老的（防内存无限增长）
        if (m_undoStack.size() > kMaxUndoDepth)
            m_undoStack.erase(m_undoStack.begin());
    }

    /// Ctrl+Z：撤销最近一条命令。返回命令名（无可撤销返回 nullptr）
    const char *undo()
    {
        if (m_undoStack.empty())
            return nullptr;
        auto cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        cmd->undo();
        const char *n = cmd->name();
        m_redoStack.push_back(std::move(cmd));
        return n;
    }

    /// Ctrl+Y：重做最近撤销的命令。返回命令名（无可重做返回 nullptr）
    const char *redo()
    {
        if (m_redoStack.empty())
            return nullptr;
        auto cmd = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        cmd->execute();
        const char *n = cmd->name();
        m_undoStack.push_back(std::move(cmd));
        return n;
    }

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    /// 清空全部历史（加载场景后调用——旧命令引用的实体已不存在）
    void clear()
    {
        m_undoStack.clear();
        m_redoStack.clear();
    }

private:
    CommandStack() = default;

    static constexpr size_t kMaxUndoDepth = 100; // 最大撤销步数

    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
};

#endif
