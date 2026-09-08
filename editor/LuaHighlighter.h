#ifndef LUAHIGHLIGHTER_H
#define LUAHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include "Scripting/ScriptApiRegistry.h" // API 名单（与补全/存根同源）
#include <vector>

// ============================================================
// LuaHighlighter —— Lua 语法高亮（QSyntaxHighlighter 子类）
//
// Qt 的语法高亮机制：把文本按行喂给 highlightBlock()，
// 我们用正则匹配出关键字/字符串/注释/数字，setFormat 染色。
// 对标 Godot 内置脚本编辑器的着色方案（深色主题）。
// ============================================================
class LuaHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit LuaHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
    {
        // ---- 关键字（蓝紫）----
        QTextCharFormat kw;
        kw.setForeground(QColor(197, 134, 192));
        kw.setFontWeight(QFont::Bold);
        const char *keywords[] = {
            "and", "break", "do", "else", "elseif", "end", "false", "for",
            "function", "if", "in", "local", "nil", "not", "or", "repeat",
            "return", "then", "true", "until", "while"};
        for (auto *k : keywords)
            m_rules.push_back({QRegularExpression(QString("\\b%1\\b").arg(k)), kw});

        // ---- 引擎注入的 API 函数（青色，帮用户认出可用接口）----
        // 直接读 ScriptApiRegistry 单一清单（不再重复维护名单）
        QTextCharFormat api;
        api.setForeground(QColor(78, 201, 176));
        for (const auto &e : scriptApiList())
            m_rules.push_back({QRegularExpression(QString("\\b%1\\b").arg(e.name)), api});

        // ---- 数字（浅绿）----
        QTextCharFormat num;
        num.setForeground(QColor(181, 206, 168));
        m_rules.push_back({QRegularExpression("\\b[0-9]+\\.?[0-9]*\\b"), num});

        // ---- 字符串（橙）：单/双引号 ----
        m_strFmt.setForeground(QColor(206, 145, 120));

        // ---- 注释（绿）：-- 到行尾 ----
        m_commentFmt.setForeground(QColor(106, 153, 85));
        m_commentRe = QRegularExpression("--[^\n]*");
    }

protected:
    void highlightBlock(const QString &text) override
    {
        // 1. 关键字/API/数字
        for (const auto &r : m_rules)
        {
            auto it = r.re.globalMatch(text);
            while (it.hasNext())
            {
                auto m = it.next();
                setFormat(m.capturedStart(), m.capturedLength(), r.fmt);
            }
        }
        // 2. 字符串（双引号 + 单引号，先于注释处理避免引号内 -- 误判）
        highlightPattern(text, QRegularExpression("\"[^\"]*\""), m_strFmt);
        highlightPattern(text, QRegularExpression("'[^']*'"), m_strFmt);
        // 3. 注释（-- 到行尾，覆盖在最上层）
        auto it = m_commentRe.globalMatch(text);
        while (it.hasNext())
        {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_commentFmt);
        }
    }

private:
    void highlightPattern(const QString &text, const QRegularExpression &re,
                          const QTextCharFormat &fmt)
    {
        auto it = re.globalMatch(text);
        while (it.hasNext())
        {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), fmt);
        }
    }

    struct Rule
    {
        QRegularExpression re;
        QTextCharFormat fmt;
    };
    std::vector<Rule> m_rules;
    QTextCharFormat m_strFmt, m_commentFmt;
    QRegularExpression m_commentRe;
};

#endif
