#include "EditorPanel.h"
#include <cstring>

// MSVC workaround: flecs::entity::get<T>() / has<T>() / get_mut<T>() bug in 4.1.6
template <typename T>
const T *comp_get(flecs::world *w, flecs::entity_t id)
{
    return static_cast<const T *>(ecs_get_id(w->c_ptr(), id, w->component<T>().raw_id()));
}
template <typename T>
T *comp_get_mut(flecs::world *w, flecs::entity_t id)
{
    return static_cast<T *>(ecs_get_mut_id(w->c_ptr(), id, w->component<T>().raw_id()));
}
template <typename T>
bool comp_has(flecs::world *w, flecs::entity_t id)
{
    return ecs_has_id(w->c_ptr(), id, w->component<T>().raw_id());
}

EditorPanel::EditorPanel(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(360);
    auto *mainLayout = new QVBoxLayout(this);
    buildEntitySection(mainLayout);
    buildComponentSection(mainLayout);
    buildPropertySection(mainLayout);
}

// ============================================================
void EditorPanel::buildEntitySection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("实体");
    auto *vlay = new QVBoxLayout(group);
    auto *btnRow = new QHBoxLayout;
    m_newBtn = new QPushButton("新建实体");
    m_deleteBtn = new QPushButton("删除");
    btnRow->addWidget(m_newBtn);
    btnRow->addWidget(m_deleteBtn);
    vlay->addLayout(btnRow);
    m_entityList = new QListWidget;
    vlay->addWidget(m_entityList);
    layout->addWidget(group);
    connect(m_newBtn, &QPushButton::clicked, this, &EditorPanel::onNewEntity);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EditorPanel::onDeleteEntity);
    connect(m_entityList, &QListWidget::currentItemChanged, this, [this]()
            { onEntitySelected(); });
}

void EditorPanel::buildComponentSection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("组件");
    auto *vlay = new QVBoxLayout(group);
    auto *addRow = new QHBoxLayout;
    m_addCompBox = new QComboBox;
    m_addCompBox->addItems({"Transform", "Mesh", "Material", "RotateBehavior"});
    m_addCompBtn = new QPushButton("+");
    m_removeCompBtn = new QPushButton("-");
    addRow->addWidget(m_addCompBox);
    addRow->addWidget(m_addCompBtn);
    addRow->addWidget(m_removeCompBtn);
    vlay->addLayout(addRow);
    m_compList = new QListWidget;
    vlay->addWidget(m_compList);
    layout->addWidget(group);
    connect(m_addCompBtn, &QPushButton::clicked, this, &EditorPanel::onAddComponent);
    connect(m_removeCompBtn, &QPushButton::clicked, this, &EditorPanel::onRemoveComponent);
    connect(m_compList, &QListWidget::currentItemChanged, this, [this]()
            { onComponentSelected(); });
}

void EditorPanel::buildPropertySection(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("属性");
    auto *vlay = new QVBoxLayout(group);
    m_propStack = new QStackedWidget;
    m_emptyProp = new QWidget;
    m_propStack->addWidget(m_emptyProp);

    auto mk = [](double min, double max, double step, double val)
    {
        auto *b = new QDoubleSpinBox;
        b->setRange(min, max);
        b->setSingleStep(step);
        b->setDecimals(2);
        b->setValue(val);
        return b;
    };

    m_transEditor = new QWidget;
    auto *tfForm = new QFormLayout(m_transEditor);
    auto addV3 = [&](QFormLayout *f, const QString &label, QDoubleSpinBox *&x,
                     QDoubleSpinBox *&y, QDoubleSpinBox *&z,
                     double min, double max, double step, double def)
    {
        auto *r = new QHBoxLayout;
        r->addWidget(new QLabel(label));
        x = mk(min, max, step, def);
        y = mk(min, max, step, def);
        z = mk(min, max, step, def);
        r->addWidget(x);
        r->addWidget(y);
        r->addWidget(z);
        f->addRow(r);
    };
    addV3(tfForm, "位置", m_posX, m_posY, m_posZ, -100, 100, 0.1, 0);
    addV3(tfForm, "旋转", m_rotX, m_rotY, m_rotZ, -180, 180, 1, 0);
    addV3(tfForm, "缩放", m_sclX, m_sclY, m_sclZ, 0.1, 100, 0.1, 1);
    m_propStack->addWidget(m_transEditor);

    m_matEditor = new QWidget;
    auto *mf = new QFormLayout(m_matEditor);
    auto addC = [&](QFormLayout *f, const QString &label, QDoubleSpinBox *&r,
                    QDoubleSpinBox *&g, QDoubleSpinBox *&b, double def)
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(label));
        r = mk(0, 1, 0.01, def);
        g = mk(0, 1, 0.01, def);
        b = mk(0, 1, 0.01, def);
        row->addWidget(r);
        row->addWidget(g);
        row->addWidget(b);
        f->addRow(row);
    };
    addC(mf, "环境光", m_ambR, m_ambG, m_ambB, 0.2);
    addC(mf, "漫反射", m_difR, m_difG, m_difB, 0.8);
    addC(mf, "镜面光", m_spcR, m_spcG, m_spcB, 1.0);
    auto *sr = new QHBoxLayout;
    sr->addWidget(new QLabel("高光"));
    m_shininess = mk(1, 256, 1, 32);
    sr->addWidget(m_shininess);
    mf->addRow(sr);
    m_propStack->addWidget(m_matEditor);

    m_rotBehEditor = new QWidget;
    auto *rf = new QFormLayout(m_rotBehEditor);
    auto *rr = new QHBoxLayout;
    rr->addWidget(new QLabel("旋转速度"));
    m_spdX = mk(-1000, 1000, 10, 0);
    rr->addWidget(new QLabel("X"));
    rr->addWidget(m_spdX);
    m_spdY = mk(-1000, 1000, 10, 0);
    rr->addWidget(new QLabel("Y"));
    rr->addWidget(m_spdY);
    m_spdZ = mk(-1000, 1000, 10, 0);
    rr->addWidget(new QLabel("Z"));
    rr->addWidget(m_spdZ);
    rf->addRow(rr);
    m_propStack->addWidget(m_rotBehEditor);

    vlay->addWidget(m_propStack);
    layout->addWidget(group);
    m_propStack->setCurrentWidget(m_emptyProp);

    auto cs = [this](QDoubleSpinBox *b)
    {
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &EditorPanel::onPropertyChanged);
    };
    cs(m_posX);
    cs(m_posY);
    cs(m_posZ);
    cs(m_rotX);
    cs(m_rotY);
    cs(m_rotZ);
    cs(m_sclX);
    cs(m_sclY);
    cs(m_sclZ);
    cs(m_ambR);
    cs(m_ambG);
    cs(m_ambB);
    cs(m_difR);
    cs(m_difG);
    cs(m_difB);
    cs(m_spcR);
    cs(m_spcG);
    cs(m_spcB);
    cs(m_shininess);
    cs(m_spdX);
    cs(m_spdY);
    cs(m_spdZ);
}

// ============================================================
void EditorPanel::onNewEntity()
{
    if (!m_world)
        return;
    auto e = m_world->entity();
    e.set<TagComponent>({"Entity"});
    e.set<TransformComponent>({});
    refreshEntityList();
}

void EditorPanel::onDeleteEntity()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    e.destruct();
    refreshEntityList();
    refreshComponentList();
    refreshPropertyEditor();
    emit valueChanged();
}

void EditorPanel::onAddComponent()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    QString name = m_addCompBox->currentText();
    if (name == "Transform")
        e.set<TransformComponent>({});
    else if (name == "Mesh")
    {
        e.set<MeshComponent>({});
        if (m_meshCreator)
            m_meshCreator(e);
    }
    else if (name == "Material")
        e.set<MaterialComponent>({});
    else if (name == "RotateBehavior")
        e.set<RotateBehavior>({});
    refreshComponentList();
    emit valueChanged();
}

void EditorPanel::onRemoveComponent()
{
    auto e = selectedEntity();
    if (!m_world || !e)
        return;
    QString name = selectedComponentName();
    if (name == "Transform")
        e.remove<TransformComponent>();
    else if (name == "Mesh")
        e.remove<MeshComponent>();
    else if (name == "Material")
        e.remove<MaterialComponent>();
    else if (name == "RotateBehavior")
        e.remove<RotateBehavior>();
    refreshComponentList();
    refreshPropertyEditor();
    emit valueChanged();
}

void EditorPanel::onEntitySelected()
{
    refreshComponentList();
    refreshPropertyEditor();
}

void EditorPanel::onComponentSelected()
{
    refreshPropertyEditor();
}

void EditorPanel::onPropertyChanged()
{
    auto e = selectedEntity();
    QString comp = selectedComponentName();
    if (!m_world || !e || comp.isEmpty())
        return;

    if (comp == "Transform")
    {
        auto *tf = comp_get_mut<TransformComponent>(m_world, e.raw_id());
        if (tf)
        {
            tf->position = {(float)m_posX->value(), (float)m_posY->value(), (float)m_posZ->value()};
            tf->rotation = {(float)m_rotX->value(), (float)m_rotY->value(), (float)m_rotZ->value()};
            tf->scale = {(float)m_sclX->value(), (float)m_sclY->value(), (float)m_sclZ->value()};
        }
    }
    else if (comp == "Material")
    {
        auto *mat = comp_get_mut<MaterialComponent>(m_world, e.raw_id());
        if (mat)
        {
            mat->ambient = {(float)m_ambR->value(), (float)m_ambG->value(), (float)m_ambB->value()};
            mat->diffuse = {(float)m_difR->value(), (float)m_difG->value(), (float)m_difB->value()};
            mat->specular = {(float)m_spcR->value(), (float)m_spcG->value(), (float)m_spcB->value()};
            mat->shininess = (float)m_shininess->value();
        }
    }
    else if (comp == "RotateBehavior")
    {
        auto *rb = comp_get_mut<RotateBehavior>(m_world, e.raw_id());
        if (rb)
        {
            rb->speedX = (float)m_spdX->value();
            rb->speedY = (float)m_spdY->value();
            rb->speedZ = (float)m_spdZ->value();
        }
    }
    emit valueChanged();
}

// ============================================================
void EditorPanel::refreshEntityList()
{
    if (!m_world)
        return;
    m_entityList->blockSignals(true);
    m_entityList->clear();

    m_world->each([this](flecs::entity e, const TagComponent &tag)
                  {
        auto *item = new QListWidgetItem(tag.name);
        item->setData(Qt::UserRole, (qulonglong)e.raw_id());
        m_entityList->addItem(item); });

    m_entityList->blockSignals(false);
}

void EditorPanel::refreshComponentList()
{
    auto e = selectedEntity();
    m_compList->clear();
    if (!m_world || !e)
        return;
    if (comp_has<TransformComponent>(m_world, e.raw_id()))
        m_compList->addItem("Transform");
    if (comp_has<MeshComponent>(m_world, e.raw_id()))
        m_compList->addItem("Mesh");
    if (comp_has<MaterialComponent>(m_world, e.raw_id()))
        m_compList->addItem("Material");
    if (comp_has<RotateBehavior>(m_world, e.raw_id()))
        m_compList->addItem("RotateBehavior");
    if (m_compList->count() > 0)
        m_compList->setCurrentRow(0);
}

void EditorPanel::refreshPropertyEditor()
{
    auto e = selectedEntity();
    QString comp = selectedComponentName();
    if (!m_world || !e || comp.isEmpty())
    {
        m_propStack->setCurrentWidget(m_emptyProp);
        return;
    }

    blockPropertySignals(true);
    if (comp == "Transform")
    {
        const auto *tf = comp_get<TransformComponent>(m_world, e.raw_id());
        if (tf)
        {
            m_posX->setValue(tf->position.x);
            m_posY->setValue(tf->position.y);
            m_posZ->setValue(tf->position.z);
            m_rotX->setValue(tf->rotation.x);
            m_rotY->setValue(tf->rotation.y);
            m_rotZ->setValue(tf->rotation.z);
            m_sclX->setValue(tf->scale.x);
            m_sclY->setValue(tf->scale.y);
            m_sclZ->setValue(tf->scale.z);
        }
        m_propStack->setCurrentWidget(m_transEditor);
    }
    else if (comp == "Material")
    {
        const auto *mat = comp_get<MaterialComponent>(m_world, e.raw_id());
        if (mat)
        {
            m_ambR->setValue(mat->ambient.x);
            m_ambG->setValue(mat->ambient.y);
            m_ambB->setValue(mat->ambient.z);
            m_difR->setValue(mat->diffuse.x);
            m_difG->setValue(mat->diffuse.y);
            m_difB->setValue(mat->diffuse.z);
            m_spcR->setValue(mat->specular.x);
            m_spcG->setValue(mat->specular.y);
            m_spcB->setValue(mat->specular.z);
            m_shininess->setValue(mat->shininess);
        }
        m_propStack->setCurrentWidget(m_matEditor);
    }
    else if (comp == "RotateBehavior")
    {
        const auto *rb = comp_get<RotateBehavior>(m_world, e.raw_id());
        if (rb)
        {
            m_spdX->setValue(rb->speedX);
            m_spdY->setValue(rb->speedY);
            m_spdZ->setValue(rb->speedZ);
        }
        m_propStack->setCurrentWidget(m_rotBehEditor);
    }
    else if (comp == "Mesh")
    {
        m_propStack->setCurrentWidget(m_emptyProp);
    }
    blockPropertySignals(false);
}

void EditorPanel::blockPropertySignals(bool block)
{
    auto b = [block](QDoubleSpinBox *s)
    { s->blockSignals(block); };
    b(m_posX);
    b(m_posY);
    b(m_posZ);
    b(m_rotX);
    b(m_rotY);
    b(m_rotZ);
    b(m_sclX);
    b(m_sclY);
    b(m_sclZ);
    b(m_ambR);
    b(m_ambG);
    b(m_ambB);
    b(m_difR);
    b(m_difG);
    b(m_difB);
    b(m_spcR);
    b(m_spcG);
    b(m_spcB);
    b(m_shininess);
    b(m_spdX);
    b(m_spdY);
    b(m_spdZ);
}

flecs::entity EditorPanel::selectedEntity() const
{
    auto *item = m_entityList->currentItem();
    if (!item || !m_world)
        return flecs::entity::null();
    return m_world->entity(item->data(Qt::UserRole).toULongLong());
}

QString EditorPanel::selectedComponentName() const
{
    auto *item = m_compList->currentItem();
    return item ? item->text() : QString();
}

bool EditorPanel::entityHasComponent(flecs::entity e, const QString &name) const
{
    if (!e)
        return false;
    if (name == "Transform")
        return comp_has<TransformComponent>(m_world, e.raw_id());
    if (name == "Mesh")
        return comp_has<MeshComponent>(m_world, e.raw_id());
    if (name == "Material")
        return comp_has<MaterialComponent>(m_world, e.raw_id());
    if (name == "RotateBehavior")
        return comp_has<RotateBehavior>(m_world, e.raw_id());
    return false;
}
