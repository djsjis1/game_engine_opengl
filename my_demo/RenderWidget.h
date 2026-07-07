#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "System/Shader.h"
#include "System/MeshFactory.h"

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit RenderWidget(QWidget *parent = nullptr);

    void setWorld(flecs::world *world) { m_world = world; }
    void cleanupGL();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    flecs::world *m_world = nullptr;
    Shader m_shader;
    MeshFactory m_factory;
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
};

#endif
