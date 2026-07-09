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
#include <cstring>
#include <vector>

// ============================================================
// 构造函数：搭建三个面板的骨架
// ============================================================
EditorPanel::EditorPanel(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(360);                       // 固定 360px 宽
    auto *mainLayout = new QVBoxLayout(this); // 垂直布局
    buildEntitySection(mainLayout);           // 第一块：实体
    buildComponentSection(mainLayout);        // 第二块：组件
    buildPropertySection(mainLayout);         // 第三块：属性
}

// ============================================================
// 实体区：[新建实体] [删除] + 实体列表
// ============================================================
void EditorPanel::buildEntitySection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("实体"); // 分组框标题
    auto *vlay = new QVBoxLayout(group);
    auto *btnRow = new QHBoxLayout;
    m_newBtn = new QPushButton("新建实体");
    m_deleteBtn = new QPushButton("删除");
    btnRow->addWidget(m_newBtn);
    btnRow->addWidget(m_deleteBtn);
    vlay->addLayout(btnRow);
    m_entityList = new QListWidget; // 实体列表
    vlay->addWidget(m_entityList);
    layout->addWidget(group);

    // 信号连接：点击按钮 → 槽函数
    connect(m_newBtn, &QPushButton::clicked, this, &EditorPanel::onNewEntity);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EditorPanel::onDeleteEntity);
    // 选中列表中某一行 → 刷新组件列表 + 属性编辑器
    connect(m_entityList, &QListWidget::currentItemChanged, this, [this]()
            { onEntitySelected(); });
}

// ============================================================
// 组件区：[下拉选组件] [+][-] + 组件列表
// ============================================================
void EditorPanel::buildComponentSection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("组件");
    auto *vlay = new QVBoxLayout(group);
    auto *addRow = new QHBoxLayout;
    m_addCompBox = new QComboBox;           // 下拉选择要添加的组件
    m_addCompBtn = new QPushButton("+");    // 添加按钮
    m_removeCompBtn = new QPushButton("-"); // 移除按钮
    addRow->addWidget(m_addCompBox);
    addRow->addWidget(m_addCompBtn);
    addRow->addWidget(m_removeCompBtn);
    vlay->addLayout(addRow);
    m_compList = new QListWidget; // 当前实体的组件列表
    vlay->addWidget(m_compList);
    layout->addWidget(group);

    connect(m_addCompBtn, &QPushButton::clicked, this, &EditorPanel::onAddComponent);
    connect(m_removeCompBtn, &QPushButton::clicked, this, &EditorPanel::onRemoveComponent);
    // 选中列表中某一行 → 刷新属性编辑器
    connect(m_compList, &QListWidget::currentItemChanged, this, [this]()
            { onComponentSelected(); });
}

// ============================================================
// 属性区：滚动区域（内容由反射动态生成）
// ============================================================
void EditorPanel::buildPropertySection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("属性");
    auto *vlay = new QVBoxLayout(group);
    m_propScroll = new QScrollArea; // 滚动区域（容纳任意高度的编辑器）
    m_propScroll->setWidgetResizable(true);
    m_emptyProp = new QWidget; // 占位空白页（没选中组件时显示）
    m_propScroll->setWidget(m_emptyProp);
    vlay->addWidget(m_propScroll);
    layout->addWidget(group);
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
// 【新建实体】
// 创建实体 → 加 TagComponent（名称）+ TransformComponent（位置）
// → 刷新实体列表
// ============================================================
void EditorPanel::onNewEntity()
{
    if (!m_world)
        return;
    auto e = m_world->entity();      // 创建空白实体
    e.set<TagComponent>({"Entity"}); // 名称 = "Entity"
    e.set<TransformComponent>({});   // 默认位置 (0,0,0)
    refreshEntityList();             // 更新 UI 中的实体列表
}

// ============================================================
// 【删除实体】
// 获取选中的实体 → destruct() 销毁 → 刷新全部面板
// ============================================================
void EditorPanel::onDeleteEntity()
{
    auto e = selectedEntity();
    if (!e)
        return;
    e.destruct();            // Flecs: 彻底销毁实体
    refreshEntityList();     // 重建实体列表
    refreshComponentList();  // 清空组件列表
    refreshPropertyEditor(); // 清空属性面板
    emit valueChanged();     // 通知 MainWindow 刷新渲染
}

// ============================================================
// 【添加组件】（反射驱动）
//
// 通用路径：e.add(compId) → Flecs 分配内存并初始化
//
// Mesh 特殊路径：Mesh 需要 OpenGL 上下文才能创建 VAO。
// → 通过 m_meshCreator 回调，让 MainWindow 调用 makeCurrent()
// → MeshFactory::createCube() 生成 GPU 网格数据
// → 回调中 e.set<MeshComponent>(mesh) 写入 VAO 句柄
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

    // ---- 2. Mesh 特殊处理 ----
    if (compId == m_world->component<MeshComponent>().raw_id())
    {
        e.set<MeshComponent>({}); // 先创建空的 MeshComponent
        if (m_meshCreator)
            m_meshCreator(e);    // 回调：创建 GPU VAO 数据
        refreshComponentList();  // 刷新组件列表
        refreshPropertyEditor(); // 刷新属性编辑区
        emit valueChanged();     // 触发重绘
        return;
    }

    // ---- 3. 通用路径：add 即可，Flecs 自动管理内存 ----
    //      add() 会分配组件内存并调用默认构造函数（POD 类型归零）
    e.add(compId);

    // 4. 刷新 UI
    refreshComponentList();
    refreshPropertyEditor();
    emit valueChanged();
}

// ============================================================
// 【移除组件】
// ============================================================
void EditorPanel::onRemoveComponent()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;

    QString name = selectedComponentName(); // 当前选中的组件名
    if (!m_componentMap.contains(name))
        return;

    flecs::entity_t compId = m_componentMap[name];
    e.remove(compId); // Flecs C++ API: 移除组件

    refreshComponentList();
    refreshPropertyEditor();
    emit valueChanged();
}

// ============================================================
// 信号槽：选中实体 → 刷新组件列表 + 属性面板
// ============================================================
void EditorPanel::onEntitySelected()
{
    refreshComponentList();  // 重建组件列表
    refreshPropertyEditor(); // 重建属性编辑器
}

// ============================================================
// 信号槽：选中组件 → 只刷新属性面板
// ============================================================
void EditorPanel::onComponentSelected()
{
    refreshPropertyEditor();
}

// ============================================================
// 【核心】属性修改：UI 控件值 → 反射写回组件内存
//
// 流程：
//   1. 获取当前的组件名和 Flecs 组件 ID
//   2. 读取反射元数据（EcsStruct），获取成员名/类型/偏移量
//   3. e.get_mut(compId) 拿到组件数据的 void* 指针
//   4. 遍历成员，根据偏移量 (m.offset) 找到字段地址
//   5. 从 m_propWidgets 查找对应的 UI 控件，读值写入
//   6. ecs_modified_id 通知 Flecs 数据已变更
//
// 为什么用 ecs_get_mut_id 而不是 e.get_mut？
// → Flecs C++ 的 e.get_mut(id_t) 内部有 assert，组件不存在会直接 abort。
//   ecs_get_mut_id 安全返回 nullptr，我们做判空处理。
// ============================================================
void EditorPanel::onPropertyChanged()
{
    if (m_updating)
        return; // 刷新锁：正在重建 UI，禁止回写

    auto e = selectedEntity();
    if (!m_world || !e)
        return;

    // 1. 获取组件信息
    QString compName = selectedComponentName(); // 如 "Transform"
    if (!m_componentMap.contains(compName))
        return;
    flecs::entity_t compId = m_componentMap[compName];
    flecs::entity compEnt(m_world->c_ptr(), compId);

    // 2. 读取反射元数据：EcsStruct 含所有成员的 offset/type/name
    auto structId2 = m_world->component<EcsStruct>().raw_id();
    const EcsStruct *st = static_cast<const EcsStruct *>(
        ecs_get_id(m_world->c_ptr(), compEnt.raw_id(), structId2));
    if (!st)
        return;

    // 3. 获取组件数据的可变指针（void* → 后面用偏移量转换为具体类型）
    void *data = ecs_get_mut_id(m_world->c_ptr(), e.raw_id(), compId);
    if (!data)
        return;

    // 4. 遍历所有成员
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
            // float / bool 成员 → 直接写入
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

    // 5. 通知 Flecs：此组件数据已被外部修改
    ecs_modified_id(m_world->c_ptr(), e.raw_id(), compId);
    emit valueChanged(); // 触发重绘
}

// ============================================================
// 【刷新实体列表】
// 遍历所有带 TagComponent 的实体，填入 QListWidget。
// 每个 item 的 UserRole 存储实体 ID，点击时通过它找回实体。
// ============================================================
void EditorPanel::refreshEntityList()
{
    if (!m_world)
        return;

    m_entityList->blockSignals(true); // 先阻断信号，防止清空时触发级联刷新
    m_entityList->clear();

    // world.each(TagComponent) → 遍历所有有 TagComponent 的实体
    m_world->each([this](flecs::entity e, const TagComponent &tag)
                  {
        auto *item = new QListWidgetItem(tag.name);     // 显示名
        // 把实体 ID 存入 UserRole（点击时用 selectedEntity() 读取）
        item->setData(Qt::UserRole, (qulonglong)e.raw_id());
        m_entityList->addItem(item); });

    m_entityList->blockSignals(false); // 恢复信号
}

// ============================================================
// 【刷新组件列表】
// 从实体类型信息中读取所有已挂载的组件，只保留在 m_componentMap
// （即 Engine 里注册过的用户组件）里的那些。
//
// 为什么 blockSignals？
// → clear() 和 setCurrentRow(0) 都会触发 currentItemChanged 信号。
//   信号会级联调用 refreshPropertyEditor()，在列表重建期间会导致
//   重入（re-entrancy）和潜在崩溃。所以用 blockSignals 整段包裹。
// ============================================================
void EditorPanel::refreshComponentList()
{
    auto e = selectedEntity();

    m_compList->blockSignals(true); // ★ 阻断信号防重入
    m_compList->clear();

    if (!m_world || !e)
    {
        m_compList->blockSignals(false);
        return;
    }

    // 获取实体的类型表：type->array 是实体上所有组件的 ID 数组
    const ecs_type_t *type = ecs_get_type(m_world->c_ptr(), e.raw_id());
    if (!type)
    {
        m_compList->blockSignals(false);
        return;
    }

    for (int i = 0; i < type->count; ++i)
    {
        flecs::entity_t compId = type->array[i];
        flecs::entity compEnt(m_world->c_ptr(), compId);
        const char *name = compEnt.name(); // 组件名
        if (!name || !name[0])
            continue;

        // 跳过内部组件
        if (compId == m_world->component<TagComponent>().raw_id())
            continue;
        if (strcmp(name, "glm::vec3") == 0)
            continue;

        // 只显示在反射注册表里的用户组件
        if (m_componentMap.contains(name))
            m_compList->addItem(name);
    }

    if (m_compList->count() > 0)
        m_compList->setCurrentRow(0); // 默认选中第一个组件
    m_compList->blockSignals(false);  // ★ 恢复信号
}

// ============================================================
// 【刷新属性编辑器】
//
// 核心流程：
//   1. 获取当前选中的组件名 → 查找 Flecs 组件 ID
//   2. 设 m_updating = true（锁住 onPropertyChanged，防止
//      创建 UI 时 setValue 触发回写）
//   3. createEditorForComponent() 动态生成 UI
//      → 读反射元数据 → 遍历成员 → 创建控件
//   4. 把生成的编辑器放入滚动区域
//   5. 解锁 m_updating
// ============================================================
void EditorPanel::refreshPropertyEditor()
{
    auto e = selectedEntity();
    QString compName = selectedComponentName();

    // 没选中组件 → 显示空白页
    if (!m_world || !e || compName.isEmpty() || !m_componentMap.contains(compName))
    {
        m_propScroll->setWidget(m_emptyProp);
        return;
    }

    flecs::entity_t compId = m_componentMap[compName];

    m_updating = true;          // ★ 上锁
    blockPropertySignals(true); // 阻止旧控件的信号
    QWidget *editor = createEditorForComponent(compId, e);
    m_propScroll->setWidget(editor); // 替换旧编辑器
    blockPropertySignals(false);
    m_updating = false; // ★ 解锁
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
    clearDynamicEditors(); // 清空上一轮的控件注册表

    // 1. 读取反射元数据
    flecs::entity compEnt(m_world->c_ptr(), compId);
    const char *compName = compEnt.name();
    QString compKey = compName ? compName : "Unknown"; // 用作 map key 前缀

    auto structId3 = m_world->component<EcsStruct>().raw_id();
    const EcsStruct *st = static_cast<const EcsStruct *>(
        ecs_get_id(m_world->c_ptr(), compEnt.raw_id(), structId3));

    // 没有反射信息 → 特殊处理 ScriptComponent（文本输入）
    if (!st)
    {
        auto scrId = m_world->component<ScriptComponent>().raw_id();
        if (compId == scrId)
        {
            auto *data = (const ScriptComponent *)ecs_get_id(m_world->c_ptr(), entity.raw_id(), compId);
            auto *w = new QWidget;
            auto *l = new QFormLayout(w);
            auto *pathEdit = new QLineEdit(data ? data->filePath : "");
            pathEdit->setPlaceholderText("scripts/xxx.lua");
            l->addRow("脚本路径", pathEdit);
            QString key = "ScriptComponent|filePath";
            m_propWidgets[key] = pathEdit;
            connect(pathEdit, &QLineEdit::textChanged, this, [this, compId, entity]()
                    {
                if (m_updating) return;
                auto *d = (ScriptComponent *)ecs_get_mut_id(m_world->c_ptr(), entity.raw_id(), compId);
                if (!d) return;
                auto *edit = qobject_cast<QLineEdit *>(m_propWidgets["ScriptComponent|filePath"]);
                if (edit) { strncpy(d->filePath, edit->text().toUtf8().constData(), sizeof(d->filePath)-1);
                    ecs_modified_id(m_world->c_ptr(), entity.raw_id(), compId); emit valueChanged(); } });
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
                auto *b = new QDoubleSpinBox;
                b->setRange(-1000, 1000);
                b->setSingleStep(0.1);
                b->setDecimals(2);
                b->setFixedWidth(70);
                return b;
            };

            const float *fv = (const float *)((const char *)data + m.offset);
            for (int vi = 0; vi < 3; ++vi)
            {
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
            auto *spin = new QDoubleSpinBox;
            spin->setRange(-10000, 10000);
            spin->setSingleStep(0.1);
            spin->setDecimals(2);

            // 从组件内存中读取当前值（用偏移量定位字段）
            spin->setValue(*(const float *)((const char *)data + m.offset));

            form->addRow(QString::fromUtf8(m.name), spin);

            QString key = compKey + "|" + m.name;
            m_propWidgets[key] = spin;
            spin->setValue(*(const float *)((const char *)data + m.offset)); // 重设
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
// 辅助：从 QListWidgetItem 的 UserRole 中读取实体 ID
// ============================================================
flecs::entity EditorPanel::selectedEntity() const
{
    auto *item = m_entityList->currentItem(); // 当前选中的列表行
    if (!item || !m_world)
        return flecs::entity::null();
    // UserRole 存的是实体 ID（raw_id），构造 flecs::entity 对象
    return m_world->entity(item->data(Qt::UserRole).toULongLong());
}

// ============================================================
// 辅助：获取组件列表中当前选中的组件名
// ============================================================
QString EditorPanel::selectedComponentName() const
{
    auto *item = m_compList->currentItem();
    return item ? item->text() : QString(); // 空列表 → 空字符串
}
