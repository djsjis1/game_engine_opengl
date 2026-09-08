// ============================================================
// CommandStack 单元测试 — 撤销/重做命令栈（命令模式）
// 覆盖：push/undo/redo/canUndo/canRedo/clear/最大深度/历史分叉
// ============================================================
#include <gtest/gtest.h>
#include "Core/CommandStack.h"

namespace
{
    /// 测试用命令：对共享计数器 +1（execute）/ -1（undo）
    class CountCommand : public Command
    {
    public:
        explicit CountCommand(int &counter, const char *name = "count")
            : m_counter(counter), m_name(name) {}
        void execute() override { ++m_counter; }
        void undo() override { --m_counter; }
        const char *name() const override { return m_name; }

    private:
        int &m_counter;
        const char *m_name;
    };
} // namespace

class CommandStackTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        CommandStack::instance().clear();
        m_counter = 0;
    }
    void TearDown() override { CommandStack::instance().clear(); }

    /// 快捷：push 一条 CountCommand
    void pushCmd(const char *name = "cmd")
    {
        CommandStack::instance().push(
            std::make_unique<CountCommand>(m_counter, name));
    }

    int m_counter = 0;
};

// ---- push + execute ----

TEST_F(CommandStackTest, Push_ExecutesCommand)
{
    pushCmd();
    EXPECT_EQ(m_counter, 1); // execute 被调用
}

TEST_F(CommandStackTest, Push_Multiple_IncrementCounter)
{
    pushCmd();
    pushCmd();
    pushCmd();
    EXPECT_EQ(m_counter, 3);
}

// ---- canUndo / canRedo ----

TEST_F(CommandStackTest, Initial_CannotUndoOrRedo)
{
    EXPECT_FALSE(CommandStack::instance().canUndo());
    EXPECT_FALSE(CommandStack::instance().canRedo());
}

TEST_F(CommandStackTest, AfterPush_CanUndo)
{
    pushCmd();
    EXPECT_TRUE(CommandStack::instance().canUndo());
    EXPECT_FALSE(CommandStack::instance().canRedo());
}

// ---- undo ----

TEST_F(CommandStackTest, Undo_RevertsEffect)
{
    pushCmd();
    EXPECT_EQ(m_counter, 1);
    CommandStack::instance().undo();
    EXPECT_EQ(m_counter, 0);
}

TEST_F(CommandStackTest, Undo_ReturnsCommandName)
{
    pushCmd("my_cmd");
    const char *name = CommandStack::instance().undo();
    EXPECT_STREQ(name, "my_cmd");
}

TEST_F(CommandStackTest, Undo_EmptyStack_ReturnsNullptr)
{
    EXPECT_EQ(CommandStack::instance().undo(), nullptr);
}

TEST_F(CommandStackTest, Undo_MultipleCommands_ReversesInOrder)
{
    pushCmd(); // counter=1
    pushCmd(); // counter=2
    pushCmd(); // counter=3
    CommandStack::instance().undo(); // counter=2
    CommandStack::instance().undo(); // counter=1
    EXPECT_EQ(m_counter, 1);
}

TEST_F(CommandStackTest, Undo_AllCommands_CounterBackToZero)
{
    pushCmd();
    pushCmd();
    CommandStack::instance().undo();
    CommandStack::instance().undo();
    EXPECT_EQ(m_counter, 0);
    EXPECT_FALSE(CommandStack::instance().canUndo());
}

// ---- redo ----

TEST_F(CommandStackTest, Redo_ReappliesEffect)
{
    pushCmd();
    CommandStack::instance().undo();
    EXPECT_EQ(m_counter, 0);
    CommandStack::instance().redo();
    EXPECT_EQ(m_counter, 1);
}

TEST_F(CommandStackTest, Redo_ReturnsCommandName)
{
    pushCmd("redo_me");
    CommandStack::instance().undo();
    const char *name = CommandStack::instance().redo();
    EXPECT_STREQ(name, "redo_me");
}

TEST_F(CommandStackTest, Redo_EmptyStack_ReturnsNullptr)
{
    EXPECT_EQ(CommandStack::instance().redo(), nullptr);
}

TEST_F(CommandStackTest, UndoRedo_MultipleSteps_StateConsistent)
{
    pushCmd(); // 1
    pushCmd(); // 2
    pushCmd(); // 3
    CommandStack::instance().undo(); // 2
    CommandStack::instance().undo(); // 1
    CommandStack::instance().redo(); // 2
    EXPECT_EQ(m_counter, 2);
    EXPECT_TRUE(CommandStack::instance().canUndo());
    EXPECT_TRUE(CommandStack::instance().canRedo());
}

// ---- 历史分叉：新命令清空重做栈 ----

TEST_F(CommandStackTest, NewCommand_ClearsRedoStack)
{
    pushCmd(); // counter=1
    pushCmd(); // counter=2
    CommandStack::instance().undo(); // counter=1, redo栈有1条

    EXPECT_TRUE(CommandStack::instance().canRedo());

    // 执行新命令 → redo 栈被清空（历史分叉）
    pushCmd(); // counter=2
    EXPECT_FALSE(CommandStack::instance().canRedo());
}

TEST_F(CommandStackTest, NewCommand_AfterUndo_RedoReturnsNull)
{
    pushCmd();
    CommandStack::instance().undo();
    pushCmd(); // 新命令顶掉旧的 redo 历史
    EXPECT_EQ(CommandStack::instance().redo(), nullptr);
}

// ---- clear ----

TEST_F(CommandStackTest, Clear_RemovesAllHistory)
{
    pushCmd();
    pushCmd();
    CommandStack::instance().undo(); // 有一条在 redo 栈
    CommandStack::instance().clear();
    EXPECT_FALSE(CommandStack::instance().canUndo());
    EXPECT_FALSE(CommandStack::instance().canRedo());
}

TEST_F(CommandStackTest, Clear_DoesNotAffectCounter)
{
    pushCmd(); // counter=1
    CommandStack::instance().clear();
    // clear 只清历史，不回滚效果
    EXPECT_EQ(m_counter, 1);
}

// ---- 最大深度限制 ----

TEST_F(CommandStackTest, MaxDepth_OldestCommandDropped)
{
    // 压入超过 kMaxUndoDepth(100) 条命令
    for (int i = 0; i < 105; ++i)
        pushCmd();
    EXPECT_EQ(m_counter, 105);

    // 只能撤销 100 次（最老的 5 条被丢弃）
    int undoCount = 0;
    while (CommandStack::instance().canUndo())
    {
        CommandStack::instance().undo();
        ++undoCount;
    }
    EXPECT_EQ(undoCount, 100);
    EXPECT_EQ(m_counter, 5); // 105 - 100 = 5（最老5条无法撤销）
}
