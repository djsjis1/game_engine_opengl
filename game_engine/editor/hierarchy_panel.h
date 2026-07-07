#ifndef GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H
#define GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include "../engine/engine.h"

// ============================================================
// HierarchyPanel: 场景层级树
//
// 显示所有实体，支持选择、删除。
// 通过信号 entitySelected 联动 Inspector 和 Viewport。
// ============================================================
class HierarchyPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HierarchyPanel(Engine *engine, QWidget *parent = nullptr)
        : QWidget(parent), m_engine(engine)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeWidget;
        m_tree->setHeaderLabels({"Name", "Type"});
        m_tree->header()->setStretchLastSection(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setRootIsDecorated(false);

        connect(m_tree, &QTreeWidget::itemClicked, this,
                [this](QTreeWidgetItem *item, int)
                {
                    auto id = item->data(0, Qt::UserRole).toULongLong();
                    ecs_entity_t e = static_cast<ecs_entity_t>(id);
                    if (e != 0)
                        emit entitySelected(e);
                });

        layout->addWidget(m_tree);

        auto *btnLayout = new QHBoxLayout;
        auto *delBtn = new QPushButton("Delete");
        connect(delBtn, &QPushButton::clicked, this, &HierarchyPanel::deleteSelected);
        btnLayout->addStretch();
        btnLayout->addWidget(delBtn);
        layout->addLayout(btnLayout);

        setMinimumWidth(200);
    }

    void refresh()
    {
        m_tree->clear();
        auto *w = m_engine->world().raw();
        for (auto e : m_engine->world().getAllEntities())
        {
            const auto *tag = static_cast<const TagComponent *>(ecs_get(w, e, TagComponent));
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, tag ? QString::fromUtf8(tag->name) : "Entity");
            item->setData(0, Qt::UserRole,
                          QVariant::fromValue(static_cast<qulonglong>(e)));

            QString type = "Entity";
            if (ecs_has(w, e, CameraComponent))
                type = "Camera";
            else if (ecs_has(w, e, LightComponent))
                type = "Light";
            else if (ecs_has(w, e, MeshComponent))
                type = "Mesh";
            item->setText(1, type);
        }
    }

    void deleteSelected()
    {
        auto items = m_tree->selectedItems();
        if (items.isEmpty())
            return;
        auto id = items[0]->data(0, Qt::UserRole).toULongLong();
        ecs_entity_t e = static_cast<ecs_entity_t>(id);
        if (e != 0)
        {
            m_engine->world().destroyEntity(e);
            refresh();
        }
    }

signals:
    void entitySelected(ecs_entity_t entity);

private:
    Engine *m_engine;
    QTreeWidget *m_tree;
};

#endif // GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H
#ifndef GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H
#define GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHeaderView>
#include "../engine/engine.h"

// ============================================================
// HierarchyPanel: 场景层级树
//
// 显示所有实体，支持选择、删除。
// 通过信号 entitySelected 联动 Inspector 和 Viewport。
// ============================================================
class HierarchyPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HierarchyPanel(Engine *engine, QWidget *parent = nullptr)
        : QWidget(parent), m_engine(engine)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeWidget;
        m_tree->setHeaderLabels({"Name", "Type"});
        m_tree->header()->setStretchLastSection(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setRootIsDecorated(false);

        connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int)
                {
            auto id = item->data(0, Qt::UserRole).toULongLong();
            ecs_entity_t e = static_cast<ecs_entity_t>(id);
            if (e != 0) emit entitySelected(e); });

        layout->addWidget(m_tree);

        auto *btnLayout = new QHBoxLayout;
        auto *delBtn = new QPushButton("Delete");
        connect(delBtn, &QPushButton::clicked, this, &HierarchyPanel::deleteSelected);
        btnLayout->addStretch();
        btnLayout->addWidget(delBtn);
        layout->addLayout(btnLayout);

        setMinimumWidth(200);
    }

    void refresh()
    {
        m_tree->clear();
        auto *w = m_engine->world().raw();
        for (auto e : m_engine->world().getAllEntities())
        {
            auto *tag = (TagComponent *)ecs_get(w, e, TagComponent);
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, tag ? QString::fromUtf8(tag->name) : "Entity");
            item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(e)));

            QString type = "Entity";
            if (ecs_has(w, e, CameraComponent))
                type = "Camera";
            else if (ecs_has(w, e, LightComponent))
                type = "Light";
            else if (ecs_has(w, e, MeshComponent))
                type = "Mesh";
            item->setText(1, type);
        }
    }

    void deleteSelected()
    {
        auto items = m_tree->selectedItems();
        if (items.isEmpty())
            return;
        auto id = items[0]->data(0, Qt::UserRole).toULongLong();
        ecs_entity_t e = static_cast<ecs_entity_t>(id);
        if (e != 0)
        {
            m_engine->world().destroyEntity(e);
            refresh();
        }
    }

signals:
    void entitySelected(ecs_entity_t entity);

private:
    Engine *m_engine;
    QTreeWidget *m_tree;
};

#endif // GAME_ENGINE_EDITOR_HIERARCHY_PANEL_H
