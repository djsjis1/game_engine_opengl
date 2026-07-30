#ifndef SCRIPTEDITOR_H
#define SCRIPTEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>

class LineNumberArea; // 行号栏（定义在 .cpp）
class QCompleter;     // 自动补全（弹 API 下拉）

// ============================================================
// ScriptEditor —— 内置 Lua 脚本编辑器（对标 Godot 脚本编辑器）
//
// 组成：
//   QPlainTextEdit（文本区）+ 左侧行号栏 + Lua 语法高亮
//   Ctrl+S 保存 / Ctrl+F 查找替换栏
//
// 【行号栏原理】QPlainTextEdit 不自带行号，经典做法是：
//   左边留出空白（setViewportMargins）→ 贴一个自绘 widget（LineNumberArea）
//   → 监听 blockCountChanged/updateRequest 同步重绘行号
// ============================================================
class ScriptEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget *parent = nullptr);

    /// 打开文件到编辑器（记录路径，供保存用）
    void loadFile(const QString &path);
    /// 保存到当前文件（Ctrl+S / 保存按钮）
    bool save();
    QString currentPath() const { return m_path; }

    /// 设置文本编码（如 "UTF-8"、"GBK"）并用新编码重新读当前文件
    /// → 乱码时切一下就能立即正确显示（不需重新打开）
    void setEncoding(const QString &codecName);
    QString encoding() const { return m_codecName; }

    // 行号栏回调（LineNumberArea 转发）
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

signals:
    void statusMessage(const QString &msg); // 保存成功等提示 → MainWindow 状态栏
    void scriptSaved(const QString &path);  // 保存成功 → 触发热重载

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override; // Ctrl+S / Ctrl+F / 补全导航

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void insertCompletion(const QString &completion); // 选中补全项 → 插入

private:
    void showFindBar();              // 弹出查找替换栏（子窗口）
    QString wordUnderCursor() const; // 光标处正在输入的词（补全前缀）

    LineNumberArea *m_lineArea = nullptr;
    QCompleter *m_completer = nullptr; // API 自动补全
    QString m_path;                    // 当前编辑的文件绝对路径
    QString m_codecName = "UTF-8";     // 读写编码（默认 UTF-8，可切 GBK 等）
};

#endif
