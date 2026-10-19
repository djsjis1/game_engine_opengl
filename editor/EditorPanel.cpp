// ============================================================
// EditorPanel.cpp —— 反射驱动的编辑栏
//
// 【它是做什么的】
//   Godot 风格的三面板编辑器：实体列表 → 组件列表 → 属性面板
//   左边 360px 宽，固定在主窗口左侧。
//
// 【核心设计：反射驱动】
//   传统方式：每加一个组件，就要手写几十行 if-else 来画 UI
//   "if (comp == Transform) { drawVec3(位置, tf->position); }"
//   "if (comp == Material)  { drawColor(环境光, mat->ambient); }"
//
//   反射方式（当前实现）：
//   Engine.h 注册一次组件的成员信息：
//     .member<float>("speedX", offsetof(RotateBehavior, speedX))
//   EditorPanel 自动遍历这些信息，根据类型创建控件：
//     float → QDoubleSpinBox
//     vec3  → 三个 QDoubleSpinBox
//     bool  → QCheckBox
//   加新组件只需改 Engine.h，EditorPanel 一行不用改。
//
// 【为什么要保留 3 处 C API】
//   1. ecs_modified_id — Flecs 无 entity_t 版本的非模板 modified()
//   2. ecs_query_init/iter — 查询 EcsStruct 元数据
//   3. ecs_vec_count/first — member 数组只能用 C API 遍历
//   其余全是 C++ API（e.add, e.remove, e.get, e.get_mut 等）。
//
// 【关键状态变量】
//   m_componentMap  — 组件名 → Flecs 组件 ID 的映射
//   m_propWidgets   — 动态创建的 UI 控件注册表
//   m_updating      — 属性刷新锁（防止 setValue 触发回写崩溃）
// ============================================================

#include "EditorPanel.h"
#include "EditorCommands.h" // 命令模式：新建/删除/设父/显隐 → 可撤销
#include "DragSpinBox.h"    // 拖拽调值数值框（Godot/Unity 手感）
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/SphereColliderComponent.h"
#include "Components/CapsuleColliderComponent.h"
#include "Render/AnimationLibrary.h" // 动画编辑：打关键帧/新建clip/保存
#include <cstring>
#include <vector>
#include <QFile>
#include <QRegularExpression>
#include <QInputDialog>
#include <QMenu>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QColor>
#include <QSlider>
#include <QTimer>

// ============================================================
// 构造函数：搭建双面板（Godot 布局）
//   本体 this   = 左侧场景树（实体层级 + 操作按钮）
//   m_inspector = 右侧 Inspector（独立 widget，MainWindow 摆到视口右边）
// ============================================================
EditorPanel::EditorPanel(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(240);                     // 左侧场景树：窄一些（Godot 风格）
    auto *mainLayout = new QVBoxLayout(this); // 垂直布局
    buildEntitySection(mainLayout);           // 上：场景树
    buildResourcePanel(mainLayout);           // 下：资源面板（目录浏览+双击复用）
    buildInspector();                         // 右：Inspector（独立 widget）
}

// ============================================================
// ============================================================
// 场景树（对标 Godot 左上）：QTreeWidget 可折叠层级 + 右键菜单
// ============================================================
void EditorPanel::buildEntitySection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("场景");
    auto *vlay = new QVBoxLayout(group);
    auto *btnRow = new QHBoxLayout;
    m_newBtn = new QPushButton("新建实体");
    m_deleteBtn = new QPushButton("删除");
    btnRow->addWidget(m_newBtn);
    btnRow->addWidget(m_deleteBtn);
    vlay->addLayout(btnRow);

    m_entitySearch = new QLineEdit;
    m_entitySearch->setPlaceholderText("搜索实体名称");
    vlay->addWidget(m_entitySearch);

    // 场景树：真正的可折叠树形（不再是手工缩进的列表）
    m_entityTree = new QTreeWidget;
    m_entityTree->setHeaderHidden(true);                       // 不显示列头
    m_entityTree->setContextMenuPolicy(Qt::CustomContextMenu); // 启用右键菜单
    vlay->addWidget(m_entityTree, 1);
    layout->addWidget(group, 2); // 场景树占左栏上方两份高

    connect(m_newBtn, &QPushButton::clicked, this, &EditorPanel::onNewEntity);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EditorPanel::onDeleteEntity);
    // 选中树节点 → 刷新 Inspector + 通知外界（相机预览小窗等）
    connect(m_entityTree, &QTreeWidget::currentItemChanged, this, [this]()
            {
        onEntitySelected();
        auto e = selectedEntity();
        emit selectionChanged(e ? (quint64)e.raw_id() : 0); });
    // 右键菜单（添加子节点/设父/解父/显隐/存预制体）
    connect(m_entityTree, &QTreeWidget::customContextMenuRequested,
            this, &EditorPanel::onTreeContextMenu);
    connect(m_entitySearch, &QLineEdit::textChanged, this,
            [this](const QString &) { applyEntityFilter(); });
}

// ============================================================
// 资源面板（对标 Godot 左下 FileSystem）：扫描资源目录，双击复用
//   prefab → 实例化到场景     scene → 加载场景
//   anim/sprite/script → 展示（拖到实体上用的功能后续可加）
// ============================================================
void EditorPanel::buildResourcePanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("资源");
    auto *vlay = new QVBoxLayout(group);
    auto *refreshBtn = new QPushButton("🔄 刷新资源");
    vlay->addWidget(refreshBtn);
    m_resourceTree = new QTreeWidget;
    m_resourceTree->setHeaderHidden(true);
    vlay->addWidget(m_resourceTree, 1);
    layout->addWidget(group, 1); // 资源面板占左栏下方一份高

    connect(refreshBtn, &QPushButton::clicked, this, &EditorPanel::refreshResourcePanel);
    connect(m_resourceTree, &QTreeWidget::itemDoubleClicked,
            this, &EditorPanel::onResourceActivated);
    refreshResourcePanel(); // 初次扫描
}

// ============================================================
// 右侧 Inspector（对标 Godot）：独立 widget，MainWindow 摆到视口右边
//
// 结构：
//   [实体名标题]
//   [滚动区：选中实体的全部组件逐个展开（每组件一个 GroupBox+✕）]
//   [底部：添加组件下拉 + 按钮]
// ============================================================
void EditorPanel::buildInspector()
{
    m_inspector = new QWidget; // 无父：MainWindow addWidget 时接管所有权
    m_inspector->setMinimumWidth(300);
    auto *vlay = new QVBoxLayout(m_inspector);

    m_inspectorTitle = new QLabel("（未选中实体）");
    m_inspectorTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    vlay->addWidget(m_inspectorTitle);

    m_propScroll = new QScrollArea; // 滚动区：容纳任意多组件的展开面板
    m_propScroll->setWidgetResizable(true);
    // 占位空白页：QScrollArea::setWidget 会在下次 setWidget 时
    // 自动 delete 旧 widget，所以空白页每次现建，绝不能复用成员指针！
    m_propScroll->setWidget(new QWidget);
    vlay->addWidget(m_propScroll, 1); // 占满剩余空间

    // 底部：添加组件（Godot Inspector 底部同款）
    auto *addRow = new QHBoxLayout;
    m_addCompBox = new QComboBox;
    m_addCompBtn = new QPushButton("➕ 添加组件");
    addRow->addWidget(m_addCompBox, 1);
    addRow->addWidget(m_addCompBtn);
    vlay->addLayout(addRow);

    connect(m_addCompBtn, &QPushButton::clicked, this, &EditorPanel::onAddComponent);
}

// ============================================================
// 【核心】重建组件下拉列表
//
// 查询所有打了 EditorVisible 标签的组件实体。
// EditorVisible 在 Engine.h 里注册——只有我们主动标记的组件
// 才会出现在这里，Flecs 内部的几百个类型被自动排除。
//
// 为什么用 C API 查询？
// → Flecs 的 C++ query_builder 不支持 EcsStruct 这类"元数据组件"查询。
//   全局扫描"所有注册了反射的组件"只能用 C API 的 ecs_query。
// ============================================================
void EditorPanel::rebuildComponentList()
{
    if (!m_world)
        return;

    // 1. 清空旧数据
    m_componentMap.clear();
    m_addCompBox->blockSignals(true);
    m_addCompBox->clear();

    // 2. 构造查询：找所有带有 EditorVisible 标签的实体
    ecs_query_desc_t qDesc = {};
    qDesc.terms[0].id = m_world->component<EditorVisible>().raw_id();
    ecs_query_t *q = ecs_query_init(m_world->c_ptr(), &qDesc);
    if (!q)
        return;

    // 3. 遍历查询结果
    ecs_iter_t it = ecs_query_iter(m_world->c_ptr(), q);
    while (ecs_query_next(&it))
    {
        for (int i = 0; i < it.count; ++i)
        {
            flecs::entity e(m_world->c_ptr(), it.entities[i]);
            const char *name = e.name(); // 组件名，如 "TransformComponent"
            if (!name || !name[0])
                continue;

            // 二次过滤：只保留大写开头（用户组件），排除小写（Flecs 内部残留）
            if (name[0] < 'A' || name[0] > 'Z')
                continue;
            if (strchr(name, ':') != nullptr)
                continue; // 排除 "glm::vec3"

            // 排除 TagComponent（它是实体名称标签，不是可编辑组件）
            if (it.entities[i] == m_world->component<TagComponent>().raw_id())
                continue;

            // 4. 加入映射表和下拉列表
            //    映射表 key=组件名, value=Flecs 组件 ID
            //    下拉列表让用户选择
            m_componentMap[name] = it.entities[i];
            m_addCompBox->addItem(QString::fromUtf8(name));
        }
    }
    ecs_query_fini(q);
    m_addCompBox->blockSignals(false);
}

// ============================================================
// 【新建实体】命令化：Ctrl+Z 可撤销
// ============================================================
void EditorPanel::onNewEntity()
{
    if (!m_world)
        return;

    // 自动编号：Entity, Entity2, Entity3...（避免重名无法区分）
    int count = 1;
    m_world->each([&](flecs::entity, const TagComponent &tag)
                  {
        if (strncmp(tag.name, "Entity", 6) == 0) count++; });

    char name[64];
    if (count == 1)
        strncpy(name, "Entity", sizeof(name) - 1);
    else
        snprintf(name, sizeof(name), "Entity%d", count);

    // 命令内部创建默认实体（Tag+Transform+Material+Mesh，立即可见）
    runCommand(std::make_unique<CreateEntityCommand>(m_world, name));

    refreshEntityList(); // 更新 UI 中的实体列表
    emit valueChanged(); // 通知重绘（新实体立即显现）
}

// ============================================================
// 【删除实体】命令化：execute=快照子树+删除，undo=完整重建
// ============================================================
void EditorPanel::onDeleteEntity()
{
    auto e = selectedEntity();
    if (!e)
        return;
    runCommand(std::make_unique<DeleteEntityCommand>(m_world, e.raw_id()));
    m_entityTree->setCurrentItem(nullptr); // 显式清除选择，避免残留死实体引用
    refreshEntityList();                   // 重建实体列表
    refreshPropertyEditor();               // 清空 Inspector
    emit valueChanged();                   // 通知 MainWindow 刷新渲染
}

// ============================================================
// 命令执行/撤销/重做
// ResourceManager 延迟化后，命令内的资源操作都是纯记账，
// 不再需要 GL 上下文包裹 —— 任何时机执行都安全
// ============================================================
void EditorPanel::runCommand(std::unique_ptr<Command> cmd)
{
    CommandStack::instance().push(std::move(cmd));
}

void EditorPanel::undoLast()
{
    const char *n = CommandStack::instance().undo();
    if (!n)
        return;                            // 无可撤销
    m_entityTree->setCurrentItem(nullptr); // 防止选中项引用已撤销的实体
    refreshEntityList();
    refreshPropertyEditor();
    emit valueChanged();
}

void EditorPanel::redoLast()
{
    const char *n = CommandStack::instance().redo();
    if (!n)
        return; // 无可重做
    m_entityTree->setCurrentItem(nullptr);
    refreshEntityList();
    refreshPropertyEditor();
    emit valueChanged();
}

// ============================================================
// 【设为子级】选中实体 → 弹窗选父实体 → Flecs child_of
// ============================================================
void EditorPanel::onSetParent()
{
    auto child = selectedEntity();
    if (!m_world || !child)
        return;

    // 收集可选父实体（排除自己和编辑器内部实体）
    QStringList names;
    std::vector<flecs::entity_t> ids;
    m_world->each([&](flecs::entity e, const TagComponent &tag)
                  {
        if (e.raw_id() == child.raw_id()) return;
        auto camId = m_world->component<CameraComponent>().raw_id();
        const auto *cam = static_cast<const CameraComponent *>(
            ecs_get_id(m_world->c_ptr(), e.raw_id(), camId));
        if (cam && cam->isEditor) return;
        names << QString::fromUtf8(tag.name);
        ids.push_back(e.raw_id()); });

    if (names.isEmpty())
        return;

    bool ok = false;
    QString sel = QInputDialog::getItem(this, "设为子级", "选择父实体:", names, 0, false, &ok);
    if (!ok)
        return;

    int idx = names.indexOf(sel);
    if (idx < 0)
        return;

    // 防止循环：不能把自己的祖先设为自己的子（简单检查：目标不能是 child 的后代）
    flecs::entity parent = m_world->entity(ids[idx]);
    for (flecs::entity p = parent; p; p = p.parent())
        if (p.raw_id() == child.raw_id())
            return; // 环！拒绝

    // 命令化：记录旧父，undo 时恢复
    flecs::entity oldP = child.parent();
    runCommand(std::make_unique<SetParentCommand>(
        m_world, child.raw_id(), oldP ? oldP.raw_id() : 0, parent.raw_id()));
    refreshEntityList();
    emit valueChanged();
}

// ============================================================
// 【解除父级】实体回到根层级（命令化，可撤销）
// ============================================================
void EditorPanel::onUnparent()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    flecs::entity oldP = e.parent();
    if (!oldP)
        return; // 本来就在根层级，不产生空命令
    runCommand(std::make_unique<SetParentCommand>(
        m_world, e.raw_id(), oldP.raw_id(), 0));
    refreshEntityList();
    emit valueChanged();
}

// ============================================================
// 【显示/隐藏】命令化（翻转操作：undo 就是再翻一次）
// ============================================================
void EditorPanel::onToggleVisible()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    runCommand(std::make_unique<ToggleVisibleCommand>(m_world, e.raw_id()));
    refreshEntityList(); // 列表更新隐藏标记
    emit valueChanged(); // 触发重绘
}

// ============================================================
// 【存预制体】选中实体（含子树）→ .prefab 文件（原型模式：存模板）
// ============================================================
void EditorPanel::onSavePrefab()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    // 默认落 prefabs/ 目录（绝对路径），存完刷新资源面板 → 可双击复用
    QString path = QFileDialog::getSaveFileName(this, "保存预制体",
                                                ASSET_DIR "prefabs/entity.prefab",
                                                "Prefab (*.prefab)");
    if (path.isEmpty())
        return;
    if (SceneSerializer::savePrefab(*m_world, e, path.toStdString()))
    {
        refreshResourcePanel(); // ★ 新预制体立即出现在资源面板
        emit valueChanged();
    }
}

// ============================================================
// 【实例化预制体】.prefab → 新实体子树（命令化，可撤销）
// ============================================================
void EditorPanel::onInstantiatePrefab()
{
    if (!m_world)
        return;
    QString path = QFileDialog::getOpenFileName(this, "实例化预制体",
                                                "", "Prefab (*.prefab)");
    if (path.isEmpty())
        return;
    runCommand(std::make_unique<InstantiatePrefabCommand>(m_world, path.toStdString()));
    refreshEntityList();
    emit valueChanged();
}

// ============================================================
// 【添加组件】（反射驱动）
//
// 通用路径：e.add(compId) → Flecs 分配内存并初始化
//
// Mesh 特殊路径：只需登记资源句柄（acquire 是纯记账，GPU 创建延迟到首帧渲染）
// ============================================================
void EditorPanel::onAddComponent()
{
    // 1. 获取选中的实体和组件
    auto e = selectedEntity();
    if (!m_world || !e)
        return;

    QString name = m_addCompBox->currentText(); // 从下拉框取的组件名
    if (!m_componentMap.contains(name))
        return; // 防止空列表时误点

    flecs::entity_t compId = m_componentMap[name]; // 查找 Flecs 组件 ID

    // Collider shapes are alternatives; keep exactly one active shape per entity.
    const auto boxId = m_world->component<BoxColliderComponent>().raw_id();
    const auto sphereId = m_world->component<SphereColliderComponent>().raw_id();
    const auto capsuleId = m_world->component<CapsuleColliderComponent>().raw_id();
    if (compId == boxId || compId == sphereId || compId == capsuleId)
    {
        if (compId != boxId) e.remove<BoxColliderComponent>();
        if (compId != sphereId) e.remove<SphereColliderComponent>();
        if (compId != capsuleId) e.remove<CapsuleColliderComponent>();
    }

    // ---- 2. Mesh 特殊处理：需要登记资源句柄（纯记账，无 GL 依赖）----
    if (compId == m_world->component<MeshComponent>().raw_id())
    {
        MeshComponent m;
        m.meshHandle = ResourceManager::instance().acquireMesh("cube");
        e.set<MeshComponent>(m);
        refreshPropertyEditor(); // Inspector 重建（新组件段落出现）
        emit valueChanged();     // 触发重绘
        return;
    }

    // ---- 3. 通用路径：add 即可，Flecs 自动管理内存 ----
    //      add() 会分配组件内存并调用默认构造函数（POD 类型归零）
    e.add(compId);

    // 4. 刷新 UI
    refreshPropertyEditor();
    emit valueChanged();
}

// ============================================================
// 【移除组件】Inspector 每个组件段落标题栏的 ✕ 按钮调用
// ============================================================
void EditorPanel::removeComponent(flecs::entity_t compId)
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;

    // 直接移除：Mesh/Sprite 资源由 OnRemove Observer 自动释放
    e.remove(compId);

    // 控件回调里不能立即重建面板（会销毁按钮自己）→ 延迟到事件循环
    QTimer::singleShot(0, this, [this]()
                       { refreshPropertyEditor(); });
    emit valueChanged();
}

// ============================================================
// 信号槽：选中实体 → 重建 Inspector（全组件展开）
// ============================================================
void EditorPanel::onEntitySelected()
{
    refreshPropertyEditor();
}

// ============================================================
// 【核心】属性修改：UI 控件值 → 反射写回组件内存
//
// Inspector 全组件展开后，任何控件变化都调这里 →
// 遍历实体上所有已展开的组件，逐个 writeBackComponent 回写。
// （控件注册 key = "组件名|字段名"，回写时按同样 key 反查控件）
// ============================================================
void EditorPanel::onPropertyChanged()
{
    if (m_updating)
        return; // 刷新锁：正在重建 UI，禁止回写

    auto e = selectedEntity();
    if (!m_world || !e)
        return;

    // 遍历实体类型表：每个已注册组件都尝试回写
    // （只有控件存在于 m_propWidgets 的字段才真正写入，其余自动跳过）
    const ecs_type_t *type = ecs_get_type(m_world->c_ptr(), e.raw_id());
    if (!type)
        return;
    for (int i = 0; i < type->count; ++i)
    {
        flecs::entity_t compId = type->array[i];
        if (ECS_IS_PAIR(compId))
            continue;
        flecs::entity compEnt(m_world->c_ptr(), compId);
        const char *name = compEnt.name();
        if (!name || !name[0] || !m_componentMap.contains(name))
            continue;
        writeBackComponent(QString::fromUtf8(name), compId, e);
    }
    emit valueChanged(); // 触发重绘
}

// ============================================================
// 按反射把单个组件的控件值写回组件内存
//
// 为什么用 ecs_get_mut_id 而不是 e.get_mut？
// → Flecs C++ 的 e.get_mut(id_t) 内部有 assert，组件不存在会直接 abort。
//   ecs_get_mut_id 安全返回 nullptr，我们做判空处理。
// ============================================================
void EditorPanel::writeBackComponent(const QString &compName, flecs::entity_t compId, flecs::entity e)
{
    flecs::entity compEnt(m_world->c_ptr(), compId);

    // 读反射元数据：EcsStruct 含所有成员的 offset/type/name
    auto ecsStructId = m_world->component<EcsStruct>().raw_id();
    const EcsStruct *st = static_cast<const EcsStruct *>(
        ecs_get_id(m_world->c_ptr(), compEnt.raw_id(), ecsStructId));
    if (!st)
    {
        // 无反射的特殊组件：ScriptComponent 的 properties 回写
        auto scrId = m_world->component<ScriptComponent>().raw_id();
        if (compId == scrId)
        {
            auto *sc = (ScriptComponent *)ecs_get_mut_id(m_world->c_ptr(), e.raw_id(), compId);
            if (sc)
            {
                for (auto it = m_propWidgets.begin(); it != m_propWidgets.end(); ++it)
                {
                    if (!it.key().startsWith("ScriptComponent|prop|"))
                        continue;
                    auto *spin = qobject_cast<QDoubleSpinBox *>(it.value());
                    if (!spin)
                        continue;
                    std::string name = it.key().mid(QString("ScriptComponent|prop|").length()).toStdString();
                    sc->properties[name] = (float)spin->value();
                }
                ecs_modified_id(m_world->c_ptr(), e.raw_id(), compId);
            }
        }
        return; // Anim/Sprite 等特殊面板自己直接写组件，不走这条路
    }

    // 组件数据可变指针（void* → 后面用偏移量转换为具体类型）
    void *data = ecs_get_mut_id(m_world->c_ptr(), e.raw_id(), compId);
    if (!data)
        return;

    // 遍历成员，从控件注册表反查控件读值写入
    int count = ecs_vec_count(&st->members);
    auto *members = (ecs_member_t *)ecs_vec_first(&st->members);

    for (int mi = 0; mi < count; ++mi)
    {
        const ecs_member_t &m = members[mi];
        flecs::entity memType(m_world->c_ptr(), m.type);
        bool isVec3 = (memType == m_world->component<glm::vec3>());

        if (isVec3)
        {
            // vec3 成员 → 从三个独立 spinbox 读值
            for (int vi = 0; vi < 3; ++vi)
            {
                QString key = compName + "|" + m.name + QString::number(vi);
                auto *spin = qobject_cast<QDoubleSpinBox *>(m_propWidgets.value(key));
                if (spin)
                    ((float *)((char *)data + m.offset))[vi] = (float)spin->value();
                //                                   ^^^^^^^^^ 指针算术：data + offset → 字段首地址
                //                                                         [vi] → vec3 的第 vi 个分量
            }
        }
        else
        {
            // float / bool / int 成员 → 直接写入
            QString key = compName + "|" + m.name;
            if (memType == m_world->component<float>())
            {
                auto *spin = qobject_cast<QDoubleSpinBox *>(m_propWidgets.value(key));
                if (spin)
                    *(float *)((char *)data + m.offset) = (float)spin->value();
            }
            else if (memType == m_world->component<bool>())
            {
                auto *cb = qobject_cast<QCheckBox *>(m_propWidgets.value(key));
                if (cb)
                    *(bool *)((char *)data + m.offset) = cb->isChecked();
            }
        }
    }

    // 通知 Flecs：此组件数据已被外部修改
    ecs_modified_id(m_world->c_ptr(), e.raw_id(), compId);
}

// ============================================================
// 【刷新场景树】
// 根实体作为顶层节点，子实体递归作为子节点（QTreeWidget 天然可折叠）
// 每个 item 的 UserRole 存实体 ID，点击时通过它找回实体。
// ============================================================
void EditorPanel::refreshEntityList()
{
    if (!m_world)
        return;

    m_entityTree->blockSignals(true); // 先阻断信号，防止清空时触发级联刷新
    m_entityTree->clear();

    // 层级显示：根实体作顶层节点，子实体递归挂到父节点下
    m_world->each([this](flecs::entity e, const TagComponent &tag)
                  {
        // 跳过编辑器内部实体（如编辑器摄像机）
        auto camId = m_world->component<CameraComponent>().raw_id();
        const auto *cam = static_cast<const CameraComponent *>(
            ecs_get_id(m_world->c_ptr(), e.raw_id(), camId));
        if (cam && cam->isEditor) return;

        // 只从根实体开始（有父的由递归处理）
        flecs::entity parent = e.parent();
        if (parent && parent.has<TagComponent>()) return;

        addEntityToTree(e, nullptr); }); // 顶层：parentItem=nullptr

    m_entityTree->expandAll();         // 默认展开全部层级
    m_entityTree->blockSignals(false); // 恢复信号
    applyEntityFilter();               // 场景重建后保留当前过滤条件
}

/// 显示匹配项及其祖先，方便在层级场景中定位名称。
void EditorPanel::applyEntityFilter()
{
    if (!m_entityTree)
        return;
    const QString query = m_entitySearch ? m_entitySearch->text().trimmed() : QString();
    std::function<bool(QTreeWidgetItem *)> filterItem = [&](QTreeWidgetItem *item)
    {
        bool descendantMatches = false;
        for (int i = 0; i < item->childCount(); ++i)
            descendantMatches = filterItem(item->child(i)) || descendantMatches;

        const bool matches = query.isEmpty() || item->text(0).contains(query, Qt::CaseInsensitive);
        const bool visible = matches || descendantMatches;
        item->setHidden(!visible);
        if (!query.isEmpty() && descendantMatches)
            item->setExpanded(true);
        return visible;
    };

    for (int i = 0; i < m_entityTree->topLevelItemCount(); ++i)
        filterItem(m_entityTree->topLevelItem(i));
}

/// 递归建树：parentItem=nullptr 时作为顶层节点，否则挂到父节点下
void EditorPanel::addEntityToTree(flecs::entity e, QTreeWidgetItem *parentItem)
{
    const auto *tag = static_cast<const TagComponent *>(ecs_get_id(
        m_world->c_ptr(), e.raw_id(), m_world->component<TagComponent>().raw_id()));
    if (!tag)
        return;

    // 隐藏的实体加标记（对标 Godot 场景树的 👁 划掉状态）
    const auto *tf = static_cast<const TransformComponent *>(ecs_get_id(
        m_world->c_ptr(), e.raw_id(), m_world->component<TransformComponent>().raw_id()));
    QString visMark = (tf && !tf->visible) ? "👁‍🗨 " : "";

    auto *item = new QTreeWidgetItem;
    item->setText(0, visMark + QString::fromUtf8(tag->name));
    item->setData(0, Qt::UserRole, (qulonglong)e.raw_id());
    if (tf && !tf->visible)
        item->setForeground(0, QColor(128, 128, 128)); // 隐藏的置灰

    if (parentItem)
        parentItem->addChild(item);
    else
        m_entityTree->addTopLevelItem(item);

    // 递归子实体（挂到 item 下）
    e.children([&](flecs::entity child)
               { addEntityToTree(child, item); });
}

// ============================================================
// 【重建 Inspector】—— Godot 风格：选中实体的全部组件一次性展开
//
// 流程：
//   1. 清控件注册表（旧控件即将被 setWidget 整体 delete）
//   2. 遍历实体的类型表，对每个已注册的用户组件：
//      createEditorForComponent 生成属性面板
//      → 包进一个 QGroupBox（标题=组件名 + ✕ 移除按钮）
//   3. 全部叠进一个垂直容器 → 放入滚动区
//
// m_propWidgets 的 key 是 "组件名|字段名"，多组件同时展开不冲突
// ============================================================
void EditorPanel::refreshPropertyEditor()
{
    auto e = selectedEntity();

    // ★ 先清控件注册表：setWidget 会 delete 旧面板（含全部子控件），
    //   不清的话 m_propWidgets 里全是悬空指针，后续遍历必崩
    clearDynamicEditors();

    // 没选中实体 → 空白页（必须现建，setWidget 会 delete 旧的）
    if (!m_world || !e)
    {
        if (m_inspectorTitle)
            m_inspectorTitle->setText("（未选中实体）");
        m_propScroll->setWidget(new QWidget);
        return;
    }

    // Inspector 标题 = 实体名
    const auto *tag = static_cast<const TagComponent *>(ecs_get_id(
        m_world->c_ptr(), e.raw_id(), m_world->component<TagComponent>().raw_id()));
    if (m_inspectorTitle)
        m_inspectorTitle->setText(tag ? QString::fromUtf8(tag->name) : "Entity");

    m_updating = true; // ★ 上锁：创建控件时 setValue 不触发回写

    // 容器：所有组件段落垂直叠放
    auto *page = new QWidget;
    auto *pageLay = new QVBoxLayout(page);
    pageLay->setSpacing(6);

    // 遍历实体类型表：每个已注册的用户组件生成一个折叠段
    const ecs_type_t *type = ecs_get_type(m_world->c_ptr(), e.raw_id());
    if (type)
    {
        for (int i = 0; i < type->count; ++i)
        {
            flecs::entity_t compId = type->array[i];
            if (ECS_IS_PAIR(compId))
                continue; // 跳过关系对（ChildOf 等）
            flecs::entity compEnt(m_world->c_ptr(), compId);
            const char *name = compEnt.name();
            if (!name || !name[0])
                continue;
            if (compId == m_world->component<TagComponent>().raw_id())
                continue; // Tag 是实体名，标题已显示
            if (!m_componentMap.contains(name))
                continue; // 只展示注册过的用户组件

            // —— 组件段落：GroupBox（标题 + ✕）+ 属性面板 ——
            auto *box = new QGroupBox(QString::fromUtf8(name));
            auto *boxLay = new QVBoxLayout(box);
            boxLay->setContentsMargins(6, 4, 6, 6);

            // 标题行右侧的移除按钮（GroupBox 自身不支持角标，放首行）
            auto *removeBtn = new QPushButton("✕ 移除组件");
            removeBtn->setMaximumWidth(100);
            boxLay->addWidget(removeBtn, 0, Qt::AlignRight);
            connect(removeBtn, &QPushButton::clicked, this,
                    [this, compId]()
                    { removeComponent(compId); });

            // 属性面板（反射自动生成 / Script·Anim·Sprite 特殊面板）
            boxLay->addWidget(createEditorForComponent(compId, e));
            pageLay->addWidget(box);
        }
    }
    pageLay->addStretch(1); // 底部留白（组件少时不拉伸段落）

    m_propScroll->setWidget(page); // 替换旧页（旧页被自动 delete）
    m_updating = false;            // ★ 解锁
}

void EditorPanel::clearDynamicEditors()
{
    m_propWidgets.clear(); // 清空控件注册表
    m_propMeta.clear();
}

// ============================================================
// 【核心】根据反射信息动态生成属性编辑器 UI
//
// 这是整个编辑栏最重要的函数。
//
// 工作原理：
//   1. 通过 compId 找到组件类型实体（compEnt）
//   2. 从 compEnt 读取 EcsStruct 反射元数据
//      → EcsStruct.members 是 ecs_vec_t，存储所有字段信息
//      → 每个 ecs_member_t 含：name(字段名), type(类型ID), offset(偏移量)
//   3. 从实体读取组件数据指针（void* data）
//   4. 遍历成员，根据类型创建不同控件：
//      vec3　→ 一行三个 QDoubleSpinBox（x/y/z）
//      float → 一个 QDoubleSpinBox
//      bool　→ 一个 QCheckBox
//   5. 每个控件注册到 m_propWidgets（key = "组件名|字段名|子索引"）
//      用于 onPropertyChanged 时反向查找控件读值
//
// 指针算术示例：
//   组件数据在内存中：
//     [position.x] [position.y] [position.z] [rotation.x] ...
//     ↑ offset=0   ↑ offset=4   ↑ offset=8   ↑ offset=12
//   data = 组件首地址
//   (float*)((char*)data + 12) → rotation.x 的地址
//   ((float*)((char*)data + 12))[1] → rotation.y
// ============================================================
QWidget *EditorPanel::createEditorForComponent(flecs::entity_t compId, flecs::entity entity)
{
    // 注：不在这里清控件注册表！Inspector 全组件展开模式下，
    // 本函数被连续调用多次（每组件一次），清表由 refreshPropertyEditor 开头统一做

    // 1. 读取反射元数据
    flecs::entity compEnt(m_world->c_ptr(), compId);
    const char *compName = compEnt.name();
    QString compKey = compName ? compName : "Unknown"; // 用作 map key 前缀

    auto ecsStructId = m_world->component<EcsStruct>().raw_id();
    const EcsStruct *st = static_cast<const EcsStruct *>(
        ecs_get_id(m_world->c_ptr(), compEnt.raw_id(), ecsStructId));

    // 特殊处理 SpriteComponent（含 char 数组 folder，反射面板做不了）
    // frame 虽有反射，但和 folder/帧数信息一起展示更友好
    if (compId == m_world->component<SpriteComponent>().raw_id())
    {
        auto *data = (const SpriteComponent *)ecs_get_id(
            m_world->c_ptr(), entity.raw_id(), compId);
        auto *w = new QWidget;
        auto *l = new QFormLayout(w);

        // 帧图目录（绝对路径，回车确认后登记并刷新面板）
        auto *dirEdit = new QLineEdit(data ? data->folder : "");
        dirEdit->setPlaceholderText("D:/.../sprites/walk（目录，图片按文件名排序为帧）");
        l->addRow("帧图目录", dirEdit);
        connect(dirEdit, &QLineEdit::editingFinished, this,
                [this, entity, compId, dirEdit]()
                {
            auto *d = (SpriteComponent *)ecs_get_mut_id(
                m_world->c_ptr(), entity.raw_id(), compId);
            if (!d) return;
            QString path = dirEdit->text().trimmed();
            if (path == QString::fromUtf8(d->folder)) return; // 没改不重建
            // 换目录：释放旧帧集引用，句柄清零（渲染时惰性重新 acquire）
            if (d->spriteHandle)
            {
                ResourceManager::instance().releaseSprite(d->spriteHandle);
                d->spriteHandle = 0;
            }
            strncpy(d->folder, path.toUtf8().constData(), sizeof(d->folder) - 1);
            d->folder[sizeof(d->folder) - 1] = '\0';
            d->frame = 0;
            QTimer::singleShot(0, this, [this]() { refreshPropertyEditor(); }); });

        // 帧集状态 + 当前帧选择
        if (data && data->folder[0])
        {
            // 预登记以便显示帧数（acquire 纯文件扫描，不碰 GL，安全）
            uint32_t h = data->spriteHandle;
            if (!h)
            {
                auto *d = (SpriteComponent *)ecs_get_mut_id(
                    m_world->c_ptr(), entity.raw_id(), compId);
                d->spriteHandle = ResourceManager::instance().acquireSprite(d->folder);
                h = d->spriteHandle;
            }
            int count = ResourceManager::instance().spriteFrameCount(h);
            auto *info = new QLabel(count > 0
                                        ? QString::asprintf("✓ 已加载 %d 帧", count)
                                        : QString("⚠ 目录不存在或没有图片"));
            info->setStyleSheet(count > 0 ? "color: green;" : "color: orange;");
            l->addRow(info);

            if (count > 0)
            {
                // 当前帧滑块（手动预览；动画播放时由轨道 SpriteComponent/frame 驱动）
                auto *frameSlider = new QSlider(Qt::Horizontal);
                frameSlider->setRange(0, count - 1);
                frameSlider->setValue(data->frame);
                auto *frameLabel = new QLabel(QString::number(data->frame));
                auto *row = new QHBoxLayout;
                row->addWidget(frameSlider, 1);
                row->addWidget(frameLabel);
                l->addRow("当前帧", row);
                connect(frameSlider, &QSlider::valueChanged, this,
                        [this, entity, compId, frameLabel](int v)
                        {
                    auto *d = (SpriteComponent *)ecs_get_mut_id(
                        m_world->c_ptr(), entity.raw_id(), compId);
                    if (!d) return;
                    d->frame = v;
                    frameLabel->setText(QString::number(v));
                    emit valueChanged(); });

                // 用法提示（帧动画 = 现成动画系统驱动 frame 字段）
                auto *hint = new QLabel(
                    "💡 做帧动画：AnimationComponent → 属性轨道选\n"
                    "   SpriteComponent/frame → 不同时刻打不同帧号");
                hint->setStyleSheet("color: gray;");
                l->addRow(hint);
            }
        }
        return w;
    }

    // 没有反射信息 → 特殊处理 ScriptComponent（文本输入）
    if (!st)
    {
        auto scrId = m_world->component<ScriptComponent>().raw_id();
        if (compId == scrId)
        {
            auto *data = (const ScriptComponent *)ecs_get_id(m_world->c_ptr(), entity.raw_id(), compId);
            auto *w = new QWidget;
            auto *l = new QFormLayout(w);

            // 脚本路径
            auto *pathEdit = new QLineEdit(data ? data->filePath : "");
            pathEdit->setPlaceholderText("D:/.../scripts/xxx.lua（建议绝对路径）");
            l->addRow("脚本路径", pathEdit);
            QString key = "ScriptComponent|filePath";
            m_propWidgets[key] = pathEdit;
            connect(pathEdit, &QLineEdit::textChanged, this, [this, compId, entity]()
                    {
                if (m_updating) return;
                auto *d = (ScriptComponent *)ecs_get_mut_id(m_world->c_ptr(), entity.raw_id(), compId);
                if (!d) return;
                auto *edit = qobject_cast<QLineEdit *>(m_propWidgets["ScriptComponent|filePath"]);
                if (edit) {
                    QByteArray utf8 = edit->text().toUtf8();
                    strncpy(d->filePath, utf8.constData(), sizeof(d->filePath) - 1);
                    d->filePath[sizeof(d->filePath) - 1] = '\0';
                    ecs_modified_id(m_world->c_ptr(), entity.raw_id(), compId); emit valueChanged(); } });

            // ---- @export 变量 ----
            if (data)
            {
                // ★ 静态解析（Godot 风格）：不需要运行脚本，
                //   直接扫描 .lua 文件里的 export("name", value) 声明，
                //   把编辑器还不知道的变量预先填入 properties。
                if (data->filePath[0])
                {
                    QFile f(QString::fromUtf8(data->filePath));
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                    {
                        QString src = QString::fromUtf8(f.readAll());
                        // 匹配 export("name", 3.14) / export('name', 42)
                        QRegularExpression re(R"(export\(\s*[\"']([A-Za-z_][A-Za-z0-9_]*)[\"']\s*,\s*(-?[0-9]+\.?[0-9]*)\s*\))");
                        auto *mutData = (ScriptComponent *)ecs_get_mut_id(m_world->c_ptr(), entity.raw_id(), compId);
                        auto it = re.globalMatch(src);
                        while (it.hasNext() && mutData)
                        {
                            auto m = it.next();
                            std::string name = m.captured(1).toStdString();
                            float defVal = m.captured(2).toFloat();
                            // 只补充新变量，不覆盖已有值（和脚本 export 语义一致）
                            if (mutData->properties.find(name) == mutData->properties.end())
                                mutData->properties[name] = defVal;
                        }
                        data = mutData ? mutData : data; // 用更新后的数据继续渲染
                    }
                }

                for (const auto &kv : data->properties)
                {
                    auto *spin = new DragSpinBox; // 拖拽调值（export 变量调平衡最常用）
                    spin->setRange(-10000, 10000);
                    spin->setDecimals(2);
                    spin->setValue(kv.second);
                    l->addRow(QString::fromStdString(kv.first), spin);
                    QString pkey = "ScriptComponent|prop|" + QString::fromStdString(kv.first);
                    m_propWidgets[pkey] = spin;
                    m_propMeta[pkey] = PropMeta{compId, 0, true, 0};
                    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                            this, &EditorPanel::onPropertyChanged);
                }
                if (data->properties.empty())
                    l->addRow(new QLabel("（脚本未定义 export 变量）"));
            }
            return w;
        }

        // 特殊处理 AnimationComponent（对标 Godot AnimationPlayer 面板）
        auto animId = m_world->component<AnimationComponent>().raw_id();
        if (compId == animId)
        {
            auto *data = (const AnimationComponent *)ecs_get_id(m_world->c_ptr(), entity.raw_id(), compId);
            auto *w = new QWidget;
            auto *l = new QFormLayout(w);

            // 通用写回辅助：取可写指针 + 通知重绘
            auto mutate = [this, compId, entity]() -> AnimationComponent *
            {
                if (m_updating)
                    return nullptr;
                return (AnimationComponent *)ecs_get_mut_id(
                    m_world->c_ptr(), entity.raw_id(), compId);
            };
            // 延迟重建面板（不能在控件回调里立即销毁控件自己 → use-after-free）
            auto rebuildLater = [this]()
            { QTimer::singleShot(0, this, [this]()
                                 { refreshPropertyEditor(); }); };

            // ---- 动画文件路径（回车/失焦后重建面板加载该库）----
            auto *fileEdit = new QLineEdit(data ? data->animFile : "");
            fileEdit->setPlaceholderText("D:/.../anims/xxx.anim（绝对路径；可不存在，保存时创建）");
            l->addRow("动画文件", fileEdit);
            m_propWidgets["AnimationComponent|animFile"] = fileEdit;
            connect(fileEdit, &QLineEdit::editingFinished, this, [this, mutate, fileEdit, rebuildLater]()
                    {
                auto *d = mutate(); if (!d) return;
                QByteArray u = fileEdit->text().toUtf8();
                if (strncmp(d->animFile, u.constData(), sizeof(d->animFile)) == 0) return;
                strncpy(d->animFile, u.constData(), sizeof(d->animFile) - 1);
                d->animFile[sizeof(d->animFile) - 1] = '\0';
                rebuildLater(); });

            // 取动画库（文件不存在也返回空库 → 可直接开始做动画）
            AnimLib *lib = (data && data->animFile[0])
                               ? AnimationLibraryManager::instance().getOrCreate(data->animFile)
                               : nullptr;

            // ---- 动画选择 + 新建 ----
            auto *clipRow = new QHBoxLayout;
            auto *clipBox = new QComboBox;
            if (lib)
                for (auto &kv : lib->clips)
                    clipBox->addItem(QString::fromStdString(kv.first));
            if (data)
                clipBox->setCurrentText(QString::fromUtf8(data->current));
            auto *newClipBtn = new QPushButton("新建");
            clipRow->addWidget(clipBox, 1);
            clipRow->addWidget(newClipBtn);
            l->addRow("动画", clipRow);
            m_propWidgets["AnimationComponent|clip"] = clipBox;

            connect(clipBox, &QComboBox::currentTextChanged, this,
                    [this, mutate, rebuildLater](const QString &name)
                    {
                auto *d = mutate(); if (!d) return;
                QByteArray u = name.toUtf8();
                if (strncmp(d->current, u.constData(), sizeof(d->current)) == 0) return;
                strncpy(d->current, u.constData(), sizeof(d->current) - 1);
                d->current[sizeof(d->current) - 1] = '\0';
                d->time = 0;
                rebuildLater(); });

            connect(newClipBtn, &QPushButton::clicked, this, [this, mutate, rebuildLater]()
                    {
                auto *d = mutate(); if (!d || !d->animFile[0]) return;
                bool ok = false;
                QString name = QInputDialog::getText(this, "新建动画", "动画名:",
                                                     QLineEdit::Normal, "anim", &ok);
                if (!ok || name.isEmpty()) return;
                AnimLib *lb = AnimationLibraryManager::instance().getOrCreate(d->animFile);
                if (!lb) return;
                lb->clips[name.toStdString()]; // 创建空 clip（默认 1 秒）
                QByteArray u = name.toUtf8();
                strncpy(d->current, u.constData(), sizeof(d->current) - 1);
                d->current[sizeof(d->current) - 1] = '\0';
                rebuildLater(); });

            // 当前编辑的 clip（后续控件都针对它）
            AnimClip *clip = nullptr;
            if (lib && data && data->current[0])
            {
                auto itc = lib->clips.find(data->current);
                if (itc != lib->clips.end())
                    clip = &itc->second;
            }

            if (clip)
            {
                // ---- 时长 ----
                auto *durSpin = new DragSpinBox;
                durSpin->setRange(0.1, 600);
                durSpin->setSingleStep(0.1);
                durSpin->setValue(clip->duration);
                l->addRow("时长(秒)", durSpin);
                m_propWidgets["AnimationComponent|dur"] = durSpin;
                connect(durSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, [this, clip](double v)
                        { if (!m_updating) clip->duration = (float)v; });

                // ---- 时间滑块（scrub：拖动即预览该时刻姿态，Godot 同款）----
                auto *slider = new QSlider(Qt::Horizontal);
                slider->setRange(0, 1000);
                float curT = data ? data->time : 0;
                slider->setValue(clip->duration > 0 ? int(curT / clip->duration * 1000) : 0);
                auto *timeLabel = new QLabel(QString::asprintf("%.2fs", curT));
                auto *timeRow = new QHBoxLayout;
                timeRow->addWidget(slider, 1);
                timeRow->addWidget(timeLabel);
                l->addRow("时间", timeRow);
                connect(slider, &QSlider::valueChanged, this,
                        [this, mutate, clip, entity, timeLabel](int v)
                        {
                    auto *d = mutate(); if (!d) return;
                    float t = clip->duration * v / 1000.0f;
                    d->time = t;
                    d->playing = false; // scrub 时停止自动播放
                    timeLabel->setText(QString::asprintf("%.2fs", t));
                    // 预览：反射写入该时刻全部轨道（不限于 Transform）
                    applyClip(*m_world, entity.raw_id(), *clip, t);
                    emit valueChanged(); });

                // ---- 缓动曲线选择（打帧时生效：本帧→下一帧的插值方式）----
                auto *easeBox = new QComboBox;
                for (int i = 0; i < (int)EasingType::kCount; ++i)
                    easeBox->addItem(QString::fromUtf8(easingName(i)));
                l->addRow("缓动曲线", easeBox);

                // ---- 自定义贝塞尔 4 参数（选"自定义贝塞尔"时显示）----
                // 用户用 4 个数字定义专属曲线（CSS cubic-bezier 同款）：
                //   控制点 P1=(x1,y1) P2=(x2,y2)，y 可超 [0,1] 制造过冲回弹
                auto *bzRow = new QWidget;
                auto *bzLay = new QHBoxLayout(bzRow);
                bzLay->setContentsMargins(0, 0, 0, 0);
                QDoubleSpinBox *bzSpin[4];
                const double bzDef[4] = {0.25, 0.1, 0.25, 1.0}; // CSS "ease"
                const char *bzName[4] = {"x1", "y1", "x2", "y2"};
                for (int bi = 0; bi < 4; ++bi)
                {
                    bzLay->addWidget(new QLabel(bzName[bi]));
                    bzSpin[bi] = new DragSpinBox;
                    // x 必须在 [0,1]（保证曲线 x 单调可求解）；y 可超界（过冲）
                    if (bi % 2 == 0)
                        bzSpin[bi]->setRange(0.0, 1.0);
                    else
                        bzSpin[bi]->setRange(-2.0, 3.0);
                    bzSpin[bi]->setSingleStep(0.05);
                    bzSpin[bi]->setDecimals(2);
                    bzSpin[bi]->setValue(bzDef[bi]);
                    bzLay->addWidget(bzSpin[bi]);
                }
                l->addRow("贝塞尔参数", bzRow);
                bzRow->setVisible(false); // 默认隐藏，选中自定义时显示
                connect(easeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, [bzRow](int idx)
                        { bzRow->setVisible(idx == (int)EasingType::kCustomBezier); });

                // ---- ⭕ 打关键帧：把实体当前姿态写入当前时间的三条轨道 ----
                auto *keyBtn = new QPushButton("⭕ 打关键帧（记录当前姿态）");
                l->addRow(keyBtn);
                connect(keyBtn, &QPushButton::clicked, this,
                        [this, mutate, clip, entity, rebuildLater, easeBox,
                         bzSpin0 = bzSpin[0], bzSpin1 = bzSpin[1],
                         bzSpin2 = bzSpin[2], bzSpin3 = bzSpin[3]]()
                        {
                    auto *d = mutate(); if (!d) return;
                    auto *tf = (const TransformComponent *)ecs_get_id(
                        m_world->c_ptr(), entity.raw_id(),
                        m_world->component<TransformComponent>().raw_id());
                    if (!tf) return;
                    float t = d->time;
                    int easing = easeBox->currentIndex(); // 选中的缓动算法
                    float bz[4] = {(float)bzSpin0->value(), (float)bzSpin1->value(),
                                   (float)bzSpin2->value(), (float)bzSpin3->value()};
                    // Transform 三轨（Property Track 反射路径写法）
                    clip->track("TransformComponent", "position").setKey(t, tf->position, easing, bz);
                    clip->track("TransformComponent", "rotation").setKey(t, tf->rotation, easing, bz);
                    clip->track("TransformComponent", "scale").setKey(t, tf->scale, easing, bz);
                    rebuildLater(); }); // 重建以刷新关键帧列表

                // ---- ➕ 属性轨道（Property Track：任意 REFLECT 字段都能动画）----
                // 下拉列出实体已挂组件的全部反射字段（来自 EcsStruct 元数据），
                // 选中后按钮把"该字段当前值"打为当前时间的关键帧
                auto *propBox = new QComboBox;
                {
                    ecs_world_t *wp = m_world->c_ptr();
                    const ecs_type_t *type = ecs_get_type(wp, entity.raw_id());
                    for (int32_t ci = 0; type && ci < type->count; ++ci)
                    {
                        ecs_id_t id = type->array[ci];
                        if (ECS_IS_PAIR(id))
                            continue; // 跳过关系对（ChildOf 等）
                        ecs_entity_t cid = (ecs_entity_t)(id & ECS_COMPONENT_MASK);
                        if (!cid)
                            continue;
                        const EcsStruct *st = (const EcsStruct *)ecs_get_id(wp, cid, ecs_id(EcsStruct));
                        if (!st)
                            continue; // 无反射的组件不可动画
                        const char *compName = ecs_get_name(wp, cid);
                        if (!compName)
                            continue;
                        auto *members = (const ecs_member_t *)ecs_vec_first(&st->members);
                        for (int32_t mi = 0; mi < ecs_vec_count(&st->members); ++mi)
                            propBox->addItem(QString("%1/%2").arg(compName, members[mi].name));
                    }
                }
                auto *propKeyBtn = new QPushButton("➕ 打属性关键帧");
                auto *propRow = new QHBoxLayout;
                propRow->addWidget(propBox, 1);
                propRow->addWidget(propKeyBtn);
                l->addRow("属性轨道", propRow);
                connect(propKeyBtn, &QPushButton::clicked, this,
                        [this, mutate, clip, entity, rebuildLater, easeBox, propBox]()
                        {
                    auto *d = mutate(); if (!d) return;
                    QStringList parts = propBox->currentText().split('/');
                    if (parts.size() != 2) return;
                    std::string comp = parts[0].toStdString();
                    std::string field = parts[1].toStdString();
                    // 读字段当前值（反射：找 offset+类型 → 读内存 → 存进 vec3）
                    ecs_world_t *wp = m_world->c_ptr();
                    ecs_entity_t cid = ecs_lookup(wp, comp.c_str());
                    if (!cid || !ecs_has_id(wp, entity.raw_id(), cid)) return;
                    const EcsStruct *st = (const EcsStruct *)ecs_get_id(wp, cid, ecs_id(EcsStruct));
                    if (!st) return;
                    const void *data = ecs_get_id(wp, entity.raw_id(), cid);
                    if (!data) return;
                    auto *members = (const ecs_member_t *)ecs_vec_first(&st->members);
                    for (int32_t mi = 0; mi < ecs_vec_count(&st->members); ++mi)
                    {
                        if (field != members[mi].name) continue;
                        const char *p = (const char *)data + members[mi].offset;
                        ecs_entity_t ft = members[mi].type;
                        glm::vec3 v(0);
                        if (ft == ecs_id(ecs_f32_t))       v.x = *(const float *)p;
                        else if (ft == ecs_id(ecs_bool_t)) v.x = *(const bool *)p ? 1.f : 0.f;
                        else if (ft == ecs_id(ecs_i32_t))  v.x = (float)*(const int32_t *)p;
                        else                               v = *(const glm::vec3 *)p;
                        auto &track = clip->track(comp, field);
                        track.setKey(d->time, v, easeBox->currentIndex());
                        track.invalidate(); // 新轨道/改动后重新解析
                        break;
                    }
                    rebuildLater(); });

                // ---- 关键帧列表（多帧管理：点击跳到该时刻，选中可删除）----
                auto *keyList = new QListWidget;
                keyList->setMaximumHeight(140);
                for (size_t ti = 0; ti < clip->tracks.size(); ++ti)
                {
                    const AnimTrack &tr = clip->tracks[ti];
                    for (size_t ki = 0; ki < tr.keys.size(); ++ki)
                    {
                        const AnimKeyframe &k = tr.keys[ki];
                        auto *item = new QListWidgetItem(QString::asprintf(
                            "%.2fs  %s (%.1f, %.1f, %.1f)  [%s]",
                            k.t, tr.trackName().c_str(), k.v.x, k.v.y, k.v.z,
                            easingName(k.easing)));
                        item->setData(Qt::UserRole, (int)ti);     // 轨道下标
                        item->setData(Qt::UserRole + 1, (int)ki); // 帧下标
                        item->setData(Qt::UserRole + 2, k.t);     // 时间（跳转用）
                        keyList->addItem(item);
                    }
                }
                l->addRow("关键帧", keyList);

                // 点击关键帧 → 时间滑块跳到该时刻（触发 scrub 预览）
                connect(keyList, &QListWidget::itemClicked, this,
                        [slider, clip](QListWidgetItem *item)
                        {
                    float t = item->data(Qt::UserRole + 2).toFloat();
                    if (clip->duration > 0)
                        slider->setValue(int(t / clip->duration * 1000)); });

                // 删除选中关键帧
                auto *delKeyBtn = new QPushButton("🗑 删除选中关键帧");
                l->addRow(delKeyBtn);
                connect(delKeyBtn, &QPushButton::clicked, this,
                        [this, clip, keyList, rebuildLater]()
                        {
                    auto *item = keyList->currentItem();
                    if (!item) return;
                    int ti = item->data(Qt::UserRole).toInt();
                    int ki = item->data(Qt::UserRole + 1).toInt();
                    if (ti >= 0 && ti < (int)clip->tracks.size())
                        clip->tracks[ti].removeKeyAt(ki);
                    rebuildLater(); });

                // ---- 事件轨道（Method Track：播到指定时刻发信号，Lua connect 接）----
                auto *evtEdit = new QLineEdit;
                evtEdit->setPlaceholderText("信号名，如 attack_hit");
                auto *evtBtn = new QPushButton("➕ 打事件帧");
                auto *evtRow = new QHBoxLayout;
                evtRow->addWidget(evtEdit, 1);
                evtRow->addWidget(evtBtn);
                l->addRow("事件轨道", evtRow);
                connect(evtBtn, &QPushButton::clicked, this,
                        [mutate, clip, evtEdit, rebuildLater]()
                        {
                    auto *d = mutate(); if (!d) return;
                    QString sig = evtEdit->text().trimmed();
                    if (sig.isEmpty()) return;
                    clip->addEvent(d->time, sig.toStdString()); // 当前时间打事件
                    rebuildLater(); });

                // 事件列表（有事件才显示）
                if (!clip->events.empty())
                {
                    auto *evtList = new QListWidget;
                    evtList->setMaximumHeight(80);
                    for (size_t ei = 0; ei < clip->events.size(); ++ei)
                    {
                        auto &ev = clip->events[ei];
                        auto *item = new QListWidgetItem(QString::asprintf(
                            "%.2fs  📢 %s", ev.t, ev.signal.c_str()));
                        item->setData(Qt::UserRole, (int)ei);
                        evtList->addItem(item);
                    }
                    l->addRow("事件", evtList);
                    auto *delEvtBtn = new QPushButton("🗑 删除选中事件");
                    l->addRow(delEvtBtn);
                    connect(delEvtBtn, &QPushButton::clicked, this,
                            [clip, evtList, rebuildLater]()
                            {
                        auto *item = evtList->currentItem();
                        if (!item) return;
                        clip->removeEventAt(item->data(Qt::UserRole).toInt());
                        rebuildLater(); });
                }

                // 删除当前动画（clip 多了需要清理废弃的）
                auto *delClipBtn = new QPushButton("🗑 删除当前动画");
                l->addRow(delClipBtn);
                connect(delClipBtn, &QPushButton::clicked, this,
                        [this, mutate, rebuildLater]()
                        {
                    auto *d = mutate(); if (!d || !d->animFile[0] || !d->current[0]) return;
                    AnimLib *lb = AnimationLibraryManager::instance().getOrCreate(d->animFile);
                    if (!lb) return;
                    lb->clips.erase(d->current);
                    d->current[0] = '\0'; // 回到未选择状态
                    d->playing = false;
                    rebuildLater(); });

                // ---- 播放预览（对标 Godot 编辑器预览）----
                auto *playChk = new QCheckBox;
                playChk->setChecked(data && data->playing);
                l->addRow("播放（预览）", playChk);
                m_propWidgets["AnimationComponent|playing"] = playChk;
                connect(playChk, &QCheckBox::toggled, this, [this, mutate](bool on)
                        {
                    auto *d = mutate(); if (!d) return;
                    d->playing = on;
                    if (on) d->time = 0.0f; // 从头预览
                    emit valueChanged(); });

                // ---- 运行时自动播放（Godot autoplay 同款：点"运行"自动开播）----
                auto *autoChk = new QCheckBox;
                autoChk->setChecked(!data || data->autoplay);
                l->addRow("运行时自动播放", autoChk);
                m_propWidgets["AnimationComponent|autoplay"] = autoChk;
                connect(autoChk, &QCheckBox::toggled, this, [this, mutate](bool on)
                        {
                    auto *d = mutate(); if (d) { d->autoplay = on; } });

                // ---- 循环 ----
                auto *loopChk = new QCheckBox;
                loopChk->setChecked(!data || data->loop);
                l->addRow("循环", loopChk);
                m_propWidgets["AnimationComponent|loop"] = loopChk;
                connect(loopChk, &QCheckBox::toggled, this, [this, mutate](bool on)
                        {
                    auto *d = mutate(); if (d) { d->loop = on; emit valueChanged(); } });

                // ---- 速度倍率 ----
                auto *speedSpin = new DragSpinBox;
                speedSpin->setRange(-10, 10);
                speedSpin->setSingleStep(0.1);
                speedSpin->setValue(data ? data->speed : 1.0);
                l->addRow("速度", speedSpin);
                m_propWidgets["AnimationComponent|speed"] = speedSpin;
                connect(speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, [this, mutate](double v)
                        {
                    auto *d = mutate(); if (d) { d->speed = (float)v; emit valueChanged(); } });

                // ---- 💾 保存到 .anim 文件 ----
                auto *saveBtn = new QPushButton("💾 保存动画文件");
                l->addRow(saveBtn);
                connect(saveBtn, &QPushButton::clicked, this, [this, mutate, saveBtn]()
                        {
                    auto *d = mutate(); if (!d || !d->animFile[0]) return;
                    bool ok = AnimationLibraryManager::instance().save(d->animFile);
                    saveBtn->setText(ok ? "💾 已保存 ✓" : "💾 保存失败 ✗"); });
            }
            else if (data && data->animFile[0])
            {
                l->addRow(new QLabel("（点“新建”创建第一个动画）"));
            }
            else
            {
                l->addRow(new QLabel("（先填写动画文件路径，回车确认）"));
            }

            return w;
        }

        auto *w = new QWidget;
        w->setLayout(new QVBoxLayout);
        w->layout()->addWidget(new QLabel("（该组件无可编辑属性）"));
        return w;
    }

    // 2. 读取组件数据（void* 指针，通过偏移量访问字段）
    const void *data = ecs_get_id(m_world->c_ptr(), entity.raw_id(), compId);
    if (!data)
    {
        auto *w = new QWidget;
        w->setLayout(new QVBoxLayout);
        w->layout()->addWidget(new QLabel("（未初始化）"));
        return w;
    }

    auto *container = new QWidget;
    auto *form = new QFormLayout(container); // 表单布局：标签-控件对

    // 3. 遍历 Flecs 反射存储的成员元数据
    int count = ecs_vec_count(&st->members);                     // 成员数量
    auto *members = (ecs_member_t *)ecs_vec_first(&st->members); // 成员数组首地址

    for (int mi = 0; mi < count; ++mi)
    {
        const ecs_member_t &m = members[mi]; // 当前成员元数据

        // 判断成员类型（通过 Flecs 组件 ID 比对）
        flecs::entity memType(m_world->c_ptr(), m.type);
        bool isVec3 = (memType == m_world->component<glm::vec3>());
        bool isFloat = (memType == m_world->component<float>());
        bool isBool = (memType == m_world->component<bool>());

        // ---- vec3：三个 spinbox 一行 ----
        if (isVec3)
        {
            auto *row = new QHBoxLayout;
            row->addWidget(new QLabel(QString::fromUtf8(m.name))); // 字段名标签

            // spinbox 工厂（不设值，避免信号触发）
            auto mk = []()
            {
                auto *b = new DragSpinBox; // 拖拽调值：位置/旋转/缩放滑着调
                b->setRange(-1000, 1000);
                b->setSingleStep(0.1);
                b->setDecimals(2);
                b->setFixedWidth(70);
                return b;
            };

            const float *fv = (const float *)((const char *)data + m.offset);
            // X/Y/Z 彩色轴标签（Godot 同款红绿蓝，一眼分清轴向）
            static const char *axisName[3] = {"X", "Y", "Z"};
            static const char *axisColor[3] = {"#e06666", "#7ec26a", "#6a9ce0"};
            for (int vi = 0; vi < 3; ++vi)
            {
                auto *axis = new QLabel(axisName[vi]);
                axis->setStyleSheet(
                    QString("color:%1; font-weight:bold;").arg(axisColor[vi]));
                row->addWidget(axis);
                auto *spin = mk();
                row->addWidget(spin);

                // 注册到控件表（key = "组件名|字段名|0/1/2"）
                QString key = compKey + "|" + m.name + QString::number(vi);
                m_propWidgets[key] = spin;

                // ★ 先注册到 map，再设值 → blockPropertySignals 可拦截信号
                spin->setValue(fv[vi]);
                connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        this, &EditorPanel::onPropertyChanged);
            }
            form->addRow(row);
        }
        // ---- float：一个 spinbox ----
        else if (isFloat)
        {
            auto *spin = new DragSpinBox; // 拖拽调值：粒子 rate/材质粗糙度滑着调
            spin->setRange(-10000, 10000);
            spin->setSingleStep(0.1);
            spin->setDecimals(2);

            form->addRow(QString::fromUtf8(m.name), spin);

            QString key = compKey + "|" + m.name;
            m_propWidgets[key] = spin;
            // 先注册再设值，防止 setValue 触发 onPropertyChanged
            spin->setValue(*(const float *)((const char *)data + m.offset));
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EditorPanel::onPropertyChanged);
        }
        // ---- bool：复选框 ----
        else if (isBool)
        {
            auto *cb = new QCheckBox;
            // 从组件内存中读取当前值
            cb->setChecked(*(const bool *)((const char *)data + m.offset));

            form->addRow(QString::fromUtf8(m.name), cb);

            QString key = compKey + "|" + m.name;
            m_propWidgets[key] = cb;
            connect(cb, &QCheckBox::toggled, this, &EditorPanel::onPropertyChanged);
        }
    }

    return container;
}

// ============================================================
// 阻塞/恢复所有动态控件的信号
// 用于重建 UI 时防止 setValue 触发 onPropertyChanged 回写
// ============================================================
void EditorPanel::blockPropertySignals(bool block)
{
    for (auto *w : m_propWidgets)
    {
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(w))
            spin->blockSignals(block);
        else if (auto *cb = qobject_cast<QCheckBox *>(w))
            cb->blockSignals(block);
    }
}

// ============================================================
// 辅助：按实体 ID 选中树节点（视口射线拾取 → 场景树同步高亮）
// setCurrentItem 会触发 currentItemChanged → 自动刷新 Inspector
// ============================================================
void EditorPanel::selectEntityById(flecs::entity_t id)
{
    if (id == 0)
    {
        m_entityTree->setCurrentItem(nullptr); // 点空白 = 取消选择
        return;
    }
    // QTreeWidgetItemIterator 递归遍历全树（含子节点）
    for (QTreeWidgetItemIterator it(m_entityTree); *it; ++it)
    {
        if ((*it)->data(0, Qt::UserRole).toULongLong() == id)
        {
            m_entityTree->setCurrentItem(*it);
            return;
        }
    }
}

// ============================================================
// 辅助：从当前选中的树节点读取实体 ID
// ============================================================
flecs::entity EditorPanel::selectedEntity() const
{
    auto *item = m_entityTree->currentItem(); // 当前选中的树节点
    if (!item || !m_world)
        return flecs::entity::null();
    // UserRole 存的是实体 ID（raw_id）
    flecs::entity_t id = item->data(0, Qt::UserRole).toULongLong();
    // ★ 必须检查 is_alive：删除后列表项可能仍引用已销毁的 ID，
    //   而 ecs_get_type/ecs_get_id 对死实体会直接崩溃（flecs::entity 的
    //   operator bool 只检查 id!=0，不检查存活，拦不住死实体）
    if (!m_world->is_alive(id))
        return flecs::entity::null();
    return m_world->entity(id);
}

// ============================================================
// 【场景树右键菜单】对标 Godot：右键节点弹出操作菜单
// ============================================================
void EditorPanel::onTreeContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto e = selectedEntity();
    bool hasSel = (bool)e; // 是否右键在某个实体上

    menu.addAction("➕ 新建子节点", this, &EditorPanel::onNewChildEntity)
        ->setEnabled(hasSel);
    menu.addAction("🔗 设为子级...", this, &EditorPanel::onSetParent)
        ->setEnabled(hasSel);
    menu.addAction("⛓ 解除父级", this, &EditorPanel::onUnparent)
        ->setEnabled(hasSel);
    menu.addSeparator();
    menu.addAction("👁 显示/隐藏", this, &EditorPanel::onToggleVisible)
        ->setEnabled(hasSel);
    menu.addAction("📦 存为预制体...", this, &EditorPanel::onSavePrefab)
        ->setEnabled(hasSel);
    menu.addSeparator();
    menu.addAction("🗑 删除", this, &EditorPanel::onDeleteEntity)
        ->setEnabled(hasSel);

    menu.exec(m_entityTree->viewport()->mapToGlobal(pos));
}

// ============================================================
// 【新建子节点】在当前选中实体下创建子实体（命令化，可撤销）
// ============================================================
void EditorPanel::onNewChildEntity()
{
    auto parent = selectedEntity();
    if (!m_world || !parent)
        return;

    // 子节点自动编号（Child, Child2...）
    int count = 1;
    m_world->each([&](flecs::entity, const TagComponent &tag)
                  {
        if (strncmp(tag.name, "Child", 5) == 0) count++; });
    char name[64];
    if (count == 1)
        strncpy(name, "Child", sizeof(name) - 1);
    else
        snprintf(name, sizeof(name), "Child%d", count);

    runCommand(std::make_unique<CreateEntityCommand>(m_world, name, parent.raw_id()));
    refreshEntityList();
    emit valueChanged();
}

// ============================================================
// 【资源面板扫描】对标 Godot FileSystem：分类列出各资源目录的文件
//
// 用绝对路径（ASSET_DIR 编译期注入，符合项目资源路径规范）扫描：
//   prefabs/ scenes/ anims/ sprites/ scripts/
// 每个文件项的 UserRole 存"绝对路径"，UserRole+1 存"类型"（双击时用）
// ============================================================
void EditorPanel::refreshResourcePanel()
{
    if (!m_resourceTree)
        return;
    m_resourceTree->clear();

    // 分类目录（相对 ASSET_DIR）+ 显示名 + 过滤后缀
    struct Cat
    {
        const char *dir;
        const char *label;
        QStringList filters;
    };
    const Cat cats[] = {
        {"prefabs", "📦 预制体", {"*.prefab"}},
        {"scenes", "🎬 场景", {"*.json"}},
        {"anims", "🎞 动画", {"*.anim"}},
        {"sprites", "🖼 精灵目录", {}}, // 精灵是目录，特殊处理
        {"scripts", "📜 脚本", {"*.lua"}},
    };

    for (const auto &cat : cats)
    {
        auto *catItem = new QTreeWidgetItem;
        catItem->setText(0, cat.label);
        m_resourceTree->addTopLevelItem(catItem);

        QString base = QString(ASSET_DIR) + cat.dir;
        QDir dir(base);
        if (!dir.exists())
            continue;

        if (QString(cat.dir) == "sprites")
        {
            // 精灵：每个子目录是一组帧
            for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            {
                auto *item = new QTreeWidgetItem;
                item->setText(0, sub);
                item->setData(0, Qt::UserRole, dir.absoluteFilePath(sub));
                item->setData(0, Qt::UserRole + 1, "sprite");
                catItem->addChild(item);
            }
        }
        else
        {
            for (const QString &f : dir.entryList(cat.filters, QDir::Files, QDir::Name))
            {
                if (f.startsWith("__"))
                    continue; // 内部文件（如 __editor_backup.json Play备份）不展示
                auto *item = new QTreeWidgetItem;
                item->setText(0, f);
                item->setData(0, Qt::UserRole, dir.absoluteFilePath(f));
                item->setData(0, Qt::UserRole + 1, cat.dir); // 类型 = 目录名
                catItem->addChild(item);
            }
        }
    }
    m_resourceTree->expandAll();
}

// ============================================================
// 【双击资源】资源复用入口（对标 Godot 双击资源）
//   prefab → 实例化到场景     scene → 加载整个场景
//   anim/sprite/script → 提示用法（拖拽到实体的功能后续可加）
// ============================================================
void EditorPanel::onResourceActivated(QTreeWidgetItem *item, int /*col*/)
{
    if (!item || !m_world)
        return;
    QString path = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (path.isEmpty())
        return; // 点的是分类标题行

    if (type == "prefabs")
    {
        // 预制体 → 实例化到场景（命令化，可撤销）
        runCommand(std::make_unique<InstantiatePrefabCommand>(m_world, path.toStdString()));
        refreshEntityList();
        emit valueChanged();
    }
    else if (type == "scenes")
    {
        // 场景 → 加载（覆盖当前场景）
        if (SceneSerializer::load(*m_world, path.toStdString()))
        {
            m_entityTree->setCurrentItem(nullptr);
            refreshEntityList();
            refreshPropertyEditor();
            emit valueChanged();
        }
    }
    // anim/sprite/script：
    else if (type == "scripts")
    {
        // 脚本 → 发信号请主窗口用内置编辑器打开（跳到 Script 页）
        emit openScriptRequested(path);
    }
    // anim/sprite：暂仅展示（后续可做拖到实体上自动挂组件）
}
