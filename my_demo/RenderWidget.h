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

struct EditorCamera;

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit RenderWidget(QWidget *parent = nullptr);
    void setWorld(flecs::world *world) { m_world = world; }
    void setEditorCamera(const EditorCamera *cam) { m_editorCam = cam; }
    void setShowGizmo(bool show) { m_showGizmo = show; }
    void cleanupGL();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void drawGizmo(const glm::mat4 &view, const glm::mat4 &proj);
    void initGizmo();

    flecs::world *m_world = nullptr;
    const EditorCamera *m_editorCam = nullptr;
    Shader m_shader;
    Shader m_lineShader;
    MeshFactory m_factory;
    GLuint m_gizmoVao = 0, m_gizmoVbo = 0;
    bool m_showGizmo = true;
};

#endif
