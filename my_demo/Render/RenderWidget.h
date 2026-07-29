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
#include "Components/SpriteComponent.h"
#include "Components/CameraComponent.h"
#include "Render/Shader.h"
#include "Render/ResourceManager.h"

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
Q_OBJECT // 信号需要（entityPicked）
    public : explicit RenderWidget(QWidget *parent = nullptr);
    void setWorld(flecs::world *world) { m_world = world; }
    void setShowGizmo(bool show) { m_showGizmo = show; }
    void cleanupGL();

signals:
    /// 射线拾取结果：左键点中实体（id=0 表示点到空白）
    /// 视口不认识编辑器面板 —— 只发信号，MainWindow 负责接线（解耦）
    void entityPicked(quint64 id);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override; // 左键→射线拾取

private:
    void drawGizmo(const glm::mat4 &view, const glm::mat4 &proj);
    void initGizmo();

    flecs::world *m_world = nullptr;
    Shader m_shader;
    Shader m_lineShader;
    GLuint m_gizmoVao = 0, m_gizmoVbo = 0;
    bool m_showGizmo = true;

    // 拾取用：paintGL 每帧存下 view/proj（鼠标点击时逆变换用）
    glm::mat4 m_lastView{1.0f};
    glm::mat4 m_lastProj{1.0f};
};

#endif
