#ifndef DRAGSPINBOX_H
#define DRAGSPINBOX_H

#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QApplication>

// ============================================================
// DragSpinBox — 可拖拽调值的数值框（对标 Godot/Unity/Blender 手感）
//
// 【交互】在输入框上按住左键水平拖动 → 数值连续增减，视口实时刷新
//   （valueChanged → onPropertyChanged → 回写组件 → 重绘，滑着调参
//    所见即所得——调粒子/灯光/材质比敲数字快得多）
//   修饰键：Ctrl = 细调(×0.1)   Shift = 粗调(×10)
//   普通点击不受影响：未拖动就释放 = 正常进入键盘编辑
//
// 【为什么继承 QDoubleSpinBox】EditorPanel 所有读值/信号代码都写的
//   qobject_cast<QDoubleSpinBox*>——子类天然匹配，替换 new 的地方即可，
//   面板逻辑零改动（里氏替换的教科书场景）。
//
// 【实现】给内部 QLineEdit 装 eventFilter：
//   按下仅记录起点；移动超过阈值才进入拖动模式（区分"点击要编辑"和
//   "拖动要调值"）；拖动中吃掉事件（阻止文本选择），释放时若拖动过
//   则吃掉释放（不弹出编辑光标）。
// ============================================================
class DragSpinBox : public QDoubleSpinBox
{
public:
    explicit DragSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent)
    {
        lineEdit()->installEventFilter(this);
        lineEdit()->setCursor(Qt::SizeHorCursor); // 提示"可以拖"
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (obj != lineEdit())
            return QDoubleSpinBox::eventFilter(obj, ev);

        switch (ev->type())
        {
        case QEvent::MouseButtonPress:
        {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton)
            {
                m_pressX = me->globalX();
                m_pressValue = value();
                m_dragging = false; // 还不知道是点击还是拖动
            }
            return false; // 先透传（点击定位光标照常）
        }
        case QEvent::MouseMove:
        {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (!(me->buttons() & Qt::LeftButton))
                return false;
            int dx = me->globalX() - m_pressX;
            if (!m_dragging && qAbs(dx) < kDragThreshold)
                return false; // 微小移动：仍当点击处理
            if (!m_dragging)
            {
                m_dragging = true;
                lineEdit()->clearFocus(); // 退出编辑态，值由拖动接管
            }
            // 每像素 = singleStep 的 1/4（Godot 手感）；Ctrl 细调 Shift 粗调
            double scale = singleStep() * 0.25;
            Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
            if (mods & Qt::ControlModifier)
                scale *= 0.1;
            if (mods & Qt::ShiftModifier)
                scale *= 10.0;
            setValue(m_pressValue + dx * scale); // 触发 valueChanged → 实时回写
            return true;                         // 吃掉：阻止拖选文本
        }
        case QEvent::MouseButtonRelease:
        {
            if (m_dragging)
            {
                m_dragging = false;
                return true; // 拖动结束：不进入编辑态
            }
            return false; // 纯点击：正常聚焦编辑
        }
        default:
            return false;
        }
    }

private:
    static constexpr int kDragThreshold = 4; // 超过 4px 才算拖动（区分点击）
    int m_pressX = 0;
    double m_pressValue = 0.0;
    bool m_dragging = false;
};

#endif
