#ifndef EDITORPANEL_H
#define EDITORPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
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

class EditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorPanel(QWidget *parent = nullptr);

    void setWorld(flecs::world *world) { m_world = world; }
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
    void blockPropertySignals(bool block);

    flecs::entity selectedEntity() const;
    QString selectedComponentName() const;
    bool entityHasComponent(flecs::entity e, const QString &name) const;

    flecs::world *m_world = nullptr;
    std::function<void(flecs::entity)> m_meshCreator;

    QListWidget *m_entityList = nullptr;
    QPushButton *m_newBtn = nullptr, *m_deleteBtn = nullptr;
    QListWidget *m_compList = nullptr;
    QComboBox *m_addCompBox = nullptr;
    QPushButton *m_addCompBtn = nullptr, *m_removeCompBtn = nullptr;

    QStackedWidget *m_propStack = nullptr;
    QWidget *m_emptyProp = nullptr;

    QWidget *m_transEditor = nullptr;
    QDoubleSpinBox *m_posX, *m_posY, *m_posZ;
    QDoubleSpinBox *m_rotX, *m_rotY, *m_rotZ;
    QDoubleSpinBox *m_sclX, *m_sclY, *m_sclZ;

    QWidget *m_matEditor = nullptr;
    QDoubleSpinBox *m_ambR, *m_ambG, *m_ambB;
    QDoubleSpinBox *m_difR, *m_difG, *m_difB;
    QDoubleSpinBox *m_spcR, *m_spcG, *m_spcB;
    QDoubleSpinBox *m_shininess;

    QWidget *m_rotBehEditor = nullptr;
    QDoubleSpinBox *m_spdX, *m_spdY, *m_spdZ;
};

#endif
