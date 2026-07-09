#ifndef EDITORPANEL_H
#define EDITORPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QMap>
#include <functional>

#ifndef Q_MOC_RUN
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#endif
#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/TagComponent.h"
#include "Components/EditorVisible.h"
#include "Components/ScriptComponent.h"

// ============================================================
// EditorPanel —— 反射驱动的编辑栏
//
// 不需要为每个组件写 if-else。
// Engine 里注册了反射后，编辑栏自动发现所有组件及其成员，
// 根据成员类型（float / vec3 / bool）自动创建合适的编辑器控件。
//
// 【加新组件只需改 Engine.h，EditorPanel 一行不用改】
// ============================================================
class EditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorPanel(QWidget *parent = nullptr);

    void setWorld(flecs::world *world)
    {
        m_world = world;
        rebuildComponentList();
    }
    void setMeshCreator(std::function<void(flecs::entity)> fn) { m_meshCreator = fn; }

signals:
    void valueChanged();

private slots:
    void onNewEntity();
    void onDeleteEntity();
    void onAddComponent();
    void onRemoveComponent();
    void onEntitySelected();
    void onComponentSelected();
    void onPropertyChanged();

private:
    void buildEntitySection(QVBoxLayout *layout);
    void buildComponentSection(QVBoxLayout *layout);
    void buildPropertySection(QVBoxLayout *layout);

    void refreshEntityList();
    void refreshComponentList();
    void refreshPropertyEditor();
    void rebuildComponentList();
    void blockPropertySignals(bool block);

    void clearDynamicEditors();
    QWidget *createEditorForComponent(flecs::entity_t compId, flecs::entity e);

    flecs::entity selectedEntity() const;
    QString selectedComponentName() const;

    flecs::world *m_world = nullptr;
    std::function<void(flecs::entity)> m_meshCreator;

    QListWidget *m_entityList = nullptr;
    QPushButton *m_newBtn = nullptr, *m_deleteBtn = nullptr;
    QListWidget *m_compList = nullptr;
    QComboBox *m_addCompBox = nullptr;
    QPushButton *m_addCompBtn = nullptr, *m_removeCompBtn = nullptr;
    QScrollArea *m_propScroll = nullptr;
    QWidget *m_emptyProp = nullptr;

    QMap<QString, flecs::entity_t> m_componentMap;
    QMap<QString, QWidget *> m_propWidgets;
    struct PropMeta
    {
        flecs::entity_t compId;
        size_t offset;
        bool isFloat;
        int vecIndex;
    };
    QMap<QString, PropMeta> m_propMeta;
    bool m_updating = false; // 属性刷新中，忽略 onPropertyChanged
};

#endif
