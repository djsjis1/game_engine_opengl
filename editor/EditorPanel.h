#ifndef EDITORPANEL_H
#define EDITORPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QTreeWidget>
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
// EditorPanel —— 反射驱动的编辑器（Godot 风格双面板）
//
// 本类管理两个物理面板（MainWindow 分别摆到视口两侧）：
//   ① 本体（this）       = 左侧场景树：实体层级 + 新建/删除/父子/预制体
//   ② inspectorWidget()  = 右侧 Inspector：选中实体的全部组件属性
//                          一次性展开（每组件一个折叠段+移除按钮），
//                          底部可添加组件 —— 对标 Godot Inspector
//
// 不需要为每个组件写 if-else：Engine 注册反射后，编辑器自动
// 发现所有组件及其成员，按类型自动创建控件。
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

    /// 右侧 Inspector 面板（MainWindow 摆到视口右边，Godot 布局）
    QWidget *inspectorWidget() const { return m_inspector; }

    /// Ctrl+Z / Ctrl+Y 入口（MainWindow 快捷键调用）
    void undoLast();
    void redoLast();

    /// 刷新实体列表（场景加载/启动时创建实体后需手动调用）
    void refreshEntityList();

    /// 按实体 ID 选中列表项（视口射线拾取后同步高亮；id=0 取消选择）
    void selectEntityById(flecs::entity_t id);

signals:
    void valueChanged();

    /// 选中实体变化（0=取消选择）——MainWindow 接给视口做相机预览等
    void selectionChanged(quint64 id);
    /// 双击脚本资源 → 请求主窗口用内置编辑器打开（解耦：编辑器不认识 MainWindow）
    void openScriptRequested(const QString &path);

private slots:
    void onNewEntity();
    void onDeleteEntity();
    void onSetParent();
    void onUnparent();
    void onToggleVisible();
    void onSavePrefab();
    void onInstantiatePrefab();
    void onAddComponent();
    void onEntitySelected();
    void onPropertyChanged();
    void onNewChildEntity();                                        // 右键菜单：在选中实体下新建子节点（对标 Godot）
    void onTreeContextMenu(const QPoint &pos);                      // 场景树右键菜单
    void onResourceActivated(class QTreeWidgetItem *item, int col); // 双击资源

public:
    /// 刷新资源面板（扫描各资源目录；保存新文件后可重扫）
    void refreshResourcePanel();

    /// 重建 Inspector：选中实体的全部组件展开显示
    /// （public：视口 Gizmo 拖动实体后需要刷新面板数值）
    void refreshPropertyEditor();

private:
    void buildEntitySection(QVBoxLayout *layout);
    void buildResourcePanel(QVBoxLayout *layout); // 资源面板（目录浏览+双击复用）
    void buildInspector();                        // 创建右侧 Inspector 面板（滚动区+底部添加组件行）

    void rebuildComponentList();
    void applyEntityFilter();
    void addEntityToTree(flecs::entity e, class QTreeWidgetItem *parentItem); // 递归建树
    void runCommand(std::unique_ptr<class Command> cmd);
    void blockPropertySignals(bool block);

    void clearDynamicEditors();
    QWidget *createEditorForComponent(flecs::entity_t compId, flecs::entity e);
    /// 按反射把某个组件的 UI 控件值回写组件内存（onPropertyChanged 逐组件调用）
    void writeBackComponent(const QString &compName, flecs::entity_t compId, flecs::entity e);
    void removeComponent(flecs::entity_t compId); // Inspector 段落✕按钮调用

    flecs::entity selectedEntity() const;

    flecs::world *m_world = nullptr;
    // 注：无任何外部回调依赖 —— ResourceManager 延迟化后，
    // 资源操作无 GL 时序要求，编辑器与渲染上下文彻底解耦

    // ---- 左面板（场景树 + 资源面板）----
    QTreeWidget *m_entityTree = nullptr;   // 场景树（可折叠层级，对标 Godot）
    QLineEdit *m_entitySearch = nullptr;   // 场景树名称过滤
    QTreeWidget *m_resourceTree = nullptr; // 资源浏览（prefab/scene/anim/sprite/script）
    QPushButton *m_newBtn = nullptr, *m_deleteBtn = nullptr;

    // ---- 右面板（Inspector，独立 widget，MainWindow 摆右侧）----
    QWidget *m_inspector = nullptr;
    QScrollArea *m_propScroll = nullptr;
    QComboBox *m_addCompBox = nullptr;
    QPushButton *m_addCompBtn = nullptr;
    QLabel *m_inspectorTitle = nullptr; // 显示当前实体名

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
