#ifndef GAME_ENGINE_EDITOR_ASSET_BROWSER_H
#define GAME_ENGINE_EDITOR_ASSET_BROWSER_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include "../engine/engine.h"

// ============================================================
// AssetBrowser: 资产浏览器
//
// 显示可用资产列表，支持导入模型(.obj)和纹理(.png/.jpg)。
// ============================================================
class AssetBrowser : public QWidget
{
    Q_OBJECT
public:
    explicit AssetBrowser(Engine *engine, QWidget *parent = nullptr)
        : QWidget(parent), m_engine(engine)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *btnLayout = new QHBoxLayout;
        auto *importModelBtn = new QPushButton("Import Model (.obj)");
        auto *importTexBtn = new QPushButton("Import Texture");
        btnLayout->addWidget(importModelBtn);
        btnLayout->addWidget(importTexBtn);
        layout->addLayout(btnLayout);

        m_listWidget = new QListWidget;
        m_listWidget->setViewMode(QListView::ListMode);
        m_listWidget->setIconSize(QSize(32, 32));
        layout->addWidget(m_listWidget);

        // 导入模型
        connect(importModelBtn, &QPushButton::clicked, this, [this]()
                {
            auto path = QFileDialog::getOpenFileName(this, "Import Model", "",
                "3D Models (*.obj *.fbx *.gltf);;All Files (*)");
            if (!path.isEmpty())
            {
                auto mesh = m_engine->meshFactory().loadOBJ(path.toStdString());
                if (mesh.isValid())
                {
                    strncpy(mesh.resourcePath, path.toUtf8().constData(), 255);
                    mesh.resourcePath[255] = '\0';

                    auto* w = m_engine->world().raw();
                    auto  e = m_engine->createEntity(
                        QFileInfo(path).baseName().toStdString());

                    TransformComponent t{};
                    ecs_set_ptr(w, e, TransformComponent, &t);
                    ecs_set_ptr(w, e, MeshComponent, &mesh);

                    MaterialComponent mat{};
                    ecs_set_ptr(w, e, MaterialComponent, &mat);

                    addItem(path, "Model");
                }
            } });

        // 导入纹理
        connect(importTexBtn, &QPushButton::clicked, this, [this]()
                {
            auto path = QFileDialog::getOpenFileName(this, "Import Texture", "",
                "Images (*.png *.jpg *.bmp);;All Files (*)");
            if (!path.isEmpty())
                addItem(path, "Texture"); });

        setMinimumHeight(120);
    }

    void addItem(const QString &path, const QString &type)
    {
        auto *item = new QListWidgetItem(
            QFileInfo(path).fileName() + " [" + type + "]");
        item->setData(Qt::UserRole, path);
        m_listWidget->addItem(item);
    }

private:
    Engine *m_engine;
    QListWidget *m_listWidget;
};

#endif // GAME_ENGINE_EDITOR_ASSET_BROWSER_H
