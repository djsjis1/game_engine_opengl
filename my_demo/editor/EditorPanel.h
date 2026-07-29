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
#include <memory>

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
#include "Components/CameraComponent.h"

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
    /// Ctrl+Z / Ctrl+Y 入口（MainWindow 快捷键调用）
    void undoLast();
    void redoLast();

    /// 刷新实体列表（场景加载/启动时创建实体后需手动调用）
    void refreshEntityList();

    /// 按实体 ID 选中列表项（视口射线拾取后同步高亮；id=0 取消选择）
    void selectEntityById(flecs::entity_t id);

signals:
    void valueChanged();

private slots:
    void onNewEntity();
    void onDeleteEntity();
    void onSetParent();
    void onUnparent();
    void onToggleVisible();
    void onSavePrefab();
    void onInstantiatePrefab();
    void onAddComponent();
    void onRemoveComponent();
    void onEntitySelected();
    void onComponentSelected();
    void onPropertyChanged();

private:
    void buildEntitySection(QVBoxLayout *layout);
    void buildComponentSection(QVBoxLayout *layout);
    void buildPropertySection(QVBoxLayout *layout);

    void refreshComponentList();
    void refreshPropertyEditor();
    void rebuildComponentList();
    void addEntityToList(flecs::entity e, int depth);
    void runCommand(std::unique_ptr<class Command> cmd); // 包 GL 上下文执行命令
    void blockPropertySignals(bool block);

    void clearDynamicEditors();
    QWidget *createEditorForComponent(flecs::entity_t compId, flecs::entity e);

    flecs::entity selectedEntity() const;
    QString selectedComponentName() const;

    flecs::world *m_world = nullptr;
    // 注：无任何外部回调依赖 —— ResourceManager 延迟化后，
    // 资源操作无 GL 时序要求，编辑器与渲染上下文彻底解耦

    QListWidget *m_entityList = nullptr;
    QPushButton *m_newBtn = nullptr, *m_deleteBtn = nullptr;
    QListWidget *m_compList = nullptr;
    QComboBox *m_addCompBox = nullptr;
    QPushButton *m_addCompBtn = nullptr, *m_removeCompBtn = nullptr;
    QScrollArea *m_propScroll = nullptr;

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
