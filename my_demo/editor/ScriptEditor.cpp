#include "ScriptEditor.h"
#include "LuaHighlighter.h"
#include "Scripting/ScriptApiRegistry.h" // API 清单（补全词库来源）

#include <QPainter>
#include <QTextBlock>
#include <QFile>
#include <QTextStream>
#include <QKeyEvent>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QTextCodec>

// 编码名 → QTextCodec（"System" 映射到系统 locale 编码，其余按名查）
static QTextCodec *resolveCodec(const QString &name)
{
    if (name == "System")
        return QTextCodec::codecForLocale();
    QTextCodec *c = QTextCodec::codecForName(name.toUtf8());
    return c ? c : QTextCodec::codecForName("UTF-8"); // 未知名回退 UTF-8
}

// ============================================================
// LineNumberArea —— 贴在编辑器左侧的行号栏（自绘小控件）
// 它不处理逻辑，只把绘制/尺寸请求转发给 ScriptEditor
// ============================================================
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(ScriptEditor *editor)
        : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    ScriptEditor *m_editor;
};

// ============================================================
ScriptEditor::ScriptEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    // 等宽字体（代码编辑器标配）+ 深色主题
    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setStyleSheet("QPlainTextEdit { background:#1e1e1e; color:#d4d4d4; }");
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' ')); // Tab=4空格宽

    // Lua 语法高亮挂到文档上
    new LuaHighlighter(document());

    // ★ API 自动补全（QCompleter）：词库来自 ScriptApiRegistry 单一清单
    QStringList words;
    for (const auto &api : scriptApiList())
        words << api.name;
    words.sort();
    m_completer = new QCompleter(words, this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &ScriptEditor::insertCompletion);

    // 行号栏
    m_lineArea = new LineNumberArea(this);
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &ScriptEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &ScriptEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &ScriptEditor::highlightCurrentLine);
    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

// ---- 行号栏宽度：按最大行号位数计算 ----
int ScriptEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance('9') * digits;
}

void ScriptEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0); // 左边留出行号栏空间
}

void ScriptEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineArea->scroll(0, dy);
    else
        m_lineArea->update(0, rect.y(), m_lineArea->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void ScriptEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineArea->setGeometry(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height());
}

// ---- 高亮当前行（编辑器常见的柔和背景条）----
void ScriptEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extras;
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(40, 40, 45));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extras.append(sel);
    }
    setExtraSelections(extras);
}

// ---- 行号栏绘制 ----
void ScriptEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineArea);
    painter.fillRect(event->rect(), QColor(0x25, 0x25, 0x26)); // 行号栏底色

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            painter.setPen(QColor(0x85, 0x85, 0x85));
            painter.drawText(0, top, m_lineArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// ---- 打开 / 保存（按 m_codecName 编码，默认 UTF-8）----
void ScriptEditor::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) // 不加 Text：避免换行符转换干扰编码
    {
        emit statusMessage("⚠ 打开失败: " + path);
        return;
    }
    QTextStream in(&f);
    in.setCodec(resolveCodec(m_codecName)); // ★ 按选定编码解码
    setPlainText(in.readAll());
    m_path = path;
    emit statusMessage("已打开: " + path + "（" + m_codecName + "）");
}

bool ScriptEditor::save()
{
    if (m_path.isEmpty())
    {
        emit statusMessage("⚠ 没有打开的文件");
        return false;
    }
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
    {
        emit statusMessage("⚠ 保存失败: " + m_path);
        return false;
    }
    QTextStream out(&f);
    out.setCodec(resolveCodec(m_codecName)); // ★ 按选定编码写入
    out << toPlainText();
    emit statusMessage("✓ 已保存: " + m_path + "（" + m_codecName + "）");
    emit scriptSaved(m_path); // 触发热重载（运行中挂此脚本的实体立即生效）
    return true;
}

// ---- 切换编码：乱码时选个对的编码，立即用新编码重读当前文件 ----
void ScriptEditor::setEncoding(const QString &codecName)
{
    if (codecName == m_codecName)
        return;
    m_codecName = codecName;
    // 有打开的文件就用新编码重新读一遍（立即消除乱码，无需手动重开）
    if (!m_path.isEmpty())
        loadFile(m_path);
    else
        emit statusMessage("编码已切为: " + m_codecName);
}

// ---- 快捷键：Ctrl+S 保存，Ctrl+F 查找替换；并驱动自动补全 ----
void ScriptEditor::keyPressEvent(QKeyEvent *event)
{
    // 补全弹窗打开时：回车/Tab 选定、Esc 关闭——交给 completer，不当普通输入
    if (m_completer && m_completer->popup()->isVisible())
    {
        switch (event->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
        case Qt::Key_Escape:
        case Qt::Key_Up:
        case Qt::Key_Down:
            event->ignore(); // 交给弹窗处理（导航/选定）
            return;
        default:
            break;
        }
    }

    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_S)
        {
            save();
            return;
        }
        if (event->key() == Qt::Key_F)
        {
            showFindBar();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event); // 正常输入

    // 输入后：取光标处的词作前缀，≥2 字符就弹补全
    QString prefix = wordUnderCursor();
    if (prefix.length() < 2)
    {
        m_completer->popup()->hide();
        return;
    }
    if (prefix != m_completer->completionPrefix())
    {
        m_completer->setCompletionPrefix(prefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }
    // 在光标处弹出下拉框
    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}

// 光标左侧正在输入的标识符（字母/数字/下划线）——补全前缀
QString ScriptEditor::wordUnderCursor() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

// 选中补全项 → 把已输入的前缀补全为完整函数名
void ScriptEditor::insertCompletion(const QString &completion)
{
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_completer->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra)); // 只插入缺的那部分
    setTextCursor(tc);
}

// ---- 查找替换栏（非模态小对话框）----
void ScriptEditor::showFindBar()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("查找/替换");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto *lay = new QHBoxLayout(dlg);

    auto *findEdit = new QLineEdit;
    findEdit->setPlaceholderText("查找");
    auto *replaceEdit = new QLineEdit;
    replaceEdit->setPlaceholderText("替换为");
    auto *findBtn = new QPushButton("查找下一个");
    auto *replaceBtn = new QPushButton("替换");
    lay->addWidget(new QLabel("🔍"));
    lay->addWidget(findEdit);
    lay->addWidget(findBtn);
    lay->addWidget(replaceEdit);
    lay->addWidget(replaceBtn);

    // 查找：从光标处向后找，找不到回到开头再找一次（循环查找）
    connect(findBtn, &QPushButton::clicked, this, [this, findEdit]()
            {
        QString kw = findEdit->text();
        if (kw.isEmpty()) return;
        if (!find(kw)) {                 // 找不到 → 回文首重试
            moveCursor(QTextCursor::Start);
            find(kw);
        } });

    // 替换：若当前选中的正是关键字则替换，然后自动查下一个
    connect(replaceBtn, &QPushButton::clicked, this, [this, findEdit, replaceEdit]()
            {
        QTextCursor c = textCursor();
        if (c.hasSelection() && c.selectedText() == findEdit->text())
            c.insertText(replaceEdit->text());
        find(findEdit->text()); });

    dlg->show(); // 非模态：可以边查边编辑
}
