// ============================================================
// demo_main.cpp — 基于引擎的演示场景
//
// 架构说明：
//   EngineWidget 是通用渲染宿主，通过 setInitCallback 注入场景。
//   回调在 initializeGL() 中引擎就绪后触发，此时 GL 上下文已激活。
// ============================================================
#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>
#include "engine_widget.h"
#include "components/transform.h"
#include "components/mesh.h"
#include "components/material.h"
#include "components/camera.h"
#include "components/light.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    EngineWidget widget;
    widget.setWindowTitle("ECS Rendering Engine — OpenGL + Qt + EnTT");
    widget.resize(1024, 768);

    // 注册初始化回调：GL 上下文就绪后构建场景
    entt::entity lightEntity{entt::null};
    entt::entity lightMarker{entt::null};

    widget.setInitCallback([&](Engine &engine)
                           {
        auto &factory = engine.meshFactory();

        // ---- 地面 ----
        auto ground = engine.createEntity();
        engine.addComponent<TransformComponent>(ground,
            QVector3D(0, -2, 0), QVector3D(0, 0, 0), QVector3D(10, 1, 10));
        engine.addComponent<MeshComponent>(ground, factory.createCube(1.0f));
        engine.addComponent<MaterialComponent>(ground,
            QVector3D(0.1f, 0.1f, 0.1f), QVector3D(0.4f, 0.4f, 0.5f),
            QVector3D(0.2f, 0.2f, 0.2f), 8.0f);

        // ---- 6 个彩色立方体 ----
        struct { QVector3D pos, ambient, diffuse, specular; float shininess; }
        cubes[] = {
            {{-2,0,-2},{0.1f,0.1f,0.1f},{0.8f,0.2f,0.2f},{0.5f,0.5f,0.5f},32},
            {{ 2,0,-2},{0.1f,0.1f,0.1f},{0.2f,0.8f,0.2f},{0.5f,0.5f,0.5f},32},
            {{-2,0, 2},{0.1f,0.1f,0.1f},{0.2f,0.2f,0.8f},{0.5f,0.5f,0.5f},32},
            {{ 2,0, 2},{0.1f,0.1f,0.1f},{0.8f,0.8f,0.2f},{0.5f,0.5f,0.5f},32},
            {{0,1.2f,-2},{0.1f,0.1f,0.1f},{0.6f,0.3f,0.7f},{0.8f,0.8f,0.8f},64},
            {{0,1.2f, 2},{0.1f,0.1f,0.1f},{0.3f,0.7f,0.6f},{0.8f,0.8f,0.8f},64},
        };

        auto cubeMesh = factory.createCube(0.8f);
        for (auto &c : cubes) {
            auto e = engine.createEntity();
            engine.addComponent<TransformComponent>(e, c.pos,
                QVector3D(0,0,0), QVector3D(1,1,1));
            engine.addComponent<MeshComponent>(e, cubeMesh);
            engine.addComponent<MaterialComponent>(e,
                c.ambient, c.diffuse, c.specular, c.shininess);
        }

        // ---- 点光源 ----
        lightEntity = engine.createEntity();
        engine.addComponent<TransformComponent>(lightEntity,
            QVector3D(2, 3, 0), QVector3D(0,0,0), QVector3D(1,1,1));
        engine.addComponent<LightComponent>(lightEntity,
            LightType::Point,
            QVector3D(0.2f,0.2f,0.2f), QVector3D(0.8f,0.8f,0.8f),
            QVector3D(1,1,1), 1.0f, 0.09f, 0.032f);

        // 光源可视化标记
        lightMarker = engine.createEntity();
        engine.addComponent<TransformComponent>(lightMarker,
            QVector3D(2, 3, 0), QVector3D(0,0,0), QVector3D(1,1,1));
        engine.addComponent<MeshComponent>(lightMarker, factory.createCube(0.15f));
        engine.addComponent<MaterialComponent>(lightMarker,
            QVector3D(1,1,1), QVector3D(1,1,1), QVector3D(1,1,1), 1.0f,
            0, 0, "light_cube");

        // ---- 摄像机 ----
        auto camEntity = engine.createEntity();
        engine.addComponent<TransformComponent>(camEntity,
            QVector3D(0, 2, 8), QVector3D(0,0,0), QVector3D(1,1,1));
        engine.addComponent<CameraComponent>(camEntity);
        widget.setCameraEntity(camEntity); });

    widget.show();

    // 旋转光源
    QTimer lightTimer;
    float lightAngle = 0;
    QObject::connect(&lightTimer, &QTimer::timeout, [&]()
                     {
        if (lightEntity == entt::null) return;
        lightAngle += 0.025f;
        float r = 3.0f;
        auto &engine = widget.engine();
        engine.getComponent<TransformComponent>(lightEntity).position =
            QVector3D(cos(lightAngle) * r, 3, sin(lightAngle) * r);
        engine.getComponent<TransformComponent>(lightMarker).position =
            engine.getComponent<TransformComponent>(lightEntity).position; });
    lightTimer.start(16);

    return app.exec();
}
