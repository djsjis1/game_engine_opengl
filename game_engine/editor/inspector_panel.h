#ifndef GAME_ENGINE_EDITOR_INSPECTOR_PANEL_H
#define GAME_ENGINE_EDITOR_INSPECTOR_PANEL_H

#include <QWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

#include <glm/glm.hpp>

#include "../engine/engine.h"

// ============================================================
// InspectorPanel: 属性检查器
//
// 选中实体后显示其组件属性，支持实时编辑。
// 修改通过 signal componentChanged 通知 Viewport 重绘。
// ============================================================
class InspectorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InspectorPanel(Engine *engine, QWidget *parent = nullptr)
        : QWidget(parent), m_engine(engine)
    {
        auto *scroll = new QScrollArea(this);
        m_formWidget = new QWidget;
        m_layout = new QFormLayout(m_formWidget);
        scroll->setWidget(m_formWidget);
        scroll->setWidgetResizable(true);

        auto *outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->addWidget(scroll);
        setMinimumWidth(250);
    }

public slots:
    void onEntitySelected(ecs_entity_t e)
    {
        m_currentEntity = e;
        refresh();
    }

signals:
    void componentChanged();

private:
    void refresh()
    {
        // 清空旧控件
        QLayoutItem *child;
        while ((child = m_layout->takeAt(0)) != nullptr)
        {
            if (child->widget())
                delete child->widget();
            delete child;
        }
        if (m_currentEntity == 0)
            return;

        auto *rawWorld = m_engine->world().raw();

        // ---- 名称 ----
        if (auto *tag = static_cast<TagComponent *>(ecs_get_mut(rawWorld, m_currentEntity, TagComponent)))
        {
            auto *nameEdit = new QLineEdit(QString::fromUtf8(tag->name));
            m_layout->addRow("Name", nameEdit);
            auto *w = rawWorld;
            auto ent = m_currentEntity;
            connect(nameEdit, &QLineEdit::textChanged, this,
                    [this, w, ent](const QString &t)
                    {
                        if (auto *tg = static_cast<TagComponent *>(ecs_get_mut(w, ent, TagComponent)))
                        {
                            qstrncpy(tg->name, t.toUtf8().constData(), 64);
                            emit componentChanged();
                        }
                    });
        }

        // ---- Transform ----
        if (auto *t = static_cast<TransformComponent *>(ecs_get_mut(rawWorld, m_currentEntity, TransformComponent)))
        {
            addVec3Editor("Position", t->position);
            addVec3Editor("Rotation", t->rotation);
            addVec3Editor("Scale", t->scale, 1.0f);
        }

        // ---- Light ----
        if (auto *l = static_cast<LightComponent *>(ecs_get_mut(rawWorld, m_currentEntity, LightComponent)))
        {
            auto *typeCombo = new QComboBox;
            typeCombo->addItems({"Directional", "Point", "Spot"});
            typeCombo->setCurrentIndex(static_cast<int>(l->type));
            m_layout->addRow("Light Type", typeCombo);
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, rawWorld](int v)
                    {
                        auto *lc = static_cast<LightComponent *>(
                            ecs_get_mut(rawWorld, m_currentEntity, LightComponent));
                        if (lc)
                        {
                            lc->type = static_cast<LightType>(v);
                            emit componentChanged();
                        }
                    });

            addVec3Editor("Ambient", l->ambient);
            addVec3Editor("Diffuse", l->diffuse);
            addVec3Editor("Specular", l->specular);
            addFloatEditor("Constant", l->constant);
            addFloatEditor("Linear", l->linear);
            addFloatEditor("Quadratic", l->quadratic);
        }

        // ---- Camera ----
        if (auto *c = static_cast<CameraComponent *>(ecs_get_mut(rawWorld, m_currentEntity, CameraComponent)))
        {
            addFloatEditor("FOV", c->fov, 1.0f, 120.0f);
            addFloatEditor("Near", c->nearPlane, 0.001f);
            addFloatEditor("Far", c->farPlane, 1.0f);
        }

        // ---- Material ----
        if (auto *m = static_cast<MaterialComponent *>(ecs_get_mut(rawWorld, m_currentEntity, MaterialComponent)))
        {
            addVec3Editor("Ambient", m->ambient);
            addVec3Editor("Diffuse", m->diffuse);
            addVec3Editor("Specular", m->specular);
            addFloatEditor("Shininess", m->shininess, 1.0f, 256.0f);
        }
    }

    // ---- 辅助：添加 vec3 编辑器（支持 glm::vec3） ----
    void addVec3Editor(const QString &label, glm::vec3 &vec, float defaultValue = 0.0f)
    {
        auto *group = new QGroupBox(label);
        auto *lay = new QHBoxLayout(group);

        auto addSpin = [&](const QString &axis, float &v)
        {
            auto *spin = new QDoubleSpinBox;
            spin->setRange(-999.0, 999.0);
            spin->setDecimals(2);
            spin->setValue(static_cast<double>(v));
            spin->setPrefix(axis + ":");
            lay->addWidget(spin);
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [&v, this](double val)
                    { v = static_cast<float>(val); emit componentChanged(); });
        };

        addSpin("X", vec.x);
        addSpin("Y", vec.y);
        addSpin("Z", vec.z);
        m_layout->addRow(group);
    }

    // ---- 辅助：添加 float 编辑器 ----
    void addFloatEditor(const QString &label, float &val, float min = 0.0f, float max = 999.0f)
    {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(static_cast<double>(min), static_cast<double>(max));
        spin->setDecimals(2);
        spin->setValue(static_cast<double>(val));
        m_layout->addRow(label, spin);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [&val, this](double v)
                { val = static_cast<float>(v); emit componentChanged(); });
    }

    Engine *m_engine;
    QWidget *m_formWidget;
    QFormLayout *m_layout;
    ecs_entity_t m_currentEntity = 0;
};

#endif // GAME_ENGINE_EDITOR_INSPECTOR_PANEL_H
