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
#include "Components/LightComponent.h"
#include "Components/EnvironmentComponent.h"
#include "Render/Shader.h"
#include "Render/ResourceManager.h"
#include "Render/RenderList.h"
#include "Render/ShadowRenderer.h"   // Pass0：方向光/聚光/点光阴影
#include "Render/PostProcessor.h"    // Pass2/3：Bloom + 色调映射
#include "Render/SkyRenderer.h"      // 天空盒
#include "Render/ParticleRenderer.h" // 粒子实例化
#include "Render/GizmoRenderer.h"    // 网格 + 变换箭头 + 拖动交互

// ============================================================
// RenderWidget — 渲染编排者（从 ECS 世界读数据，调度各子系统绘制）
//
// 【架构】子系统各自持有 GL 资源、单一职责（解耦后 RenderWidget 只负责）：
//   1. 编排 paintGL 的多 Pass 顺序
//   2. 相机选择（Play/Edit 策略）
//   3. 光源收集 + 场景实体遍历（数据 → shader uniform）
//   4. 射线拾取 + 事件分发（把 Gizmo 事件转给 GizmoRenderer）
//
// 【子系统】ShadowRenderer / PostProcessor / SkyRenderer /
//           ParticleRenderer / GizmoRenderer
// ============================================================
class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
Q_OBJECT // 信号需要（entityPicked）
    public : explicit RenderWidget(QWidget *parent = nullptr);
    void setWorld(flecs::world *world) { m_world = world; }
    void setShowGizmo(bool show) { m_showGizmo = show; }
    /// Play 模式标志：决定相机选择策略（Play 用游戏相机，Edit 用编辑器相机）
    void setPlayMode(bool play) { m_playMode = play; }
    /// 相机预览小窗：传入带游戏相机的实体（0=关闭）
    void setPreviewCamera(flecs::entity_t id) { m_previewCam = id; }
    /// 变换 Gizmo：选中实体（0=隐藏箭头）+ 模式（0=移动 1=旋转 2=缩放）
    void setGizmoTarget(flecs::entity_t id) { m_gizmo.setTarget(id); }
    void setGizmoMode(int mode) { m_gizmo.setMode(mode); }
    void cleanupGL();

signals:
    /// 射线拾取结果：左键点中实体（id=0 表示点到空白）
    void entityPicked(quint64 id);
    /// Gizmo 拖动完成：mode(0/1/2) + 旧/新三分量值（MainWindow 包成撤销命令）
    void gizmoTransformed(quint64 id, int mode, float ox, float oy, float oz,
                          float nx, float ny, float nz);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override; // gizmo 拖动 / 射线拾取
    void keyPressEvent(QKeyEvent *event) override;     // 键盘直接进 Input
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;    // gizmo 拖动中
    void mouseReleaseEvent(QMouseEvent *event) override; // gizmo 拖动结束

private:
    void drawCameraPreview(); // 右下角游戏相机预览小窗（选中相机实体时）

    flecs::world *m_world = nullptr;

    // ---- 主场景 shader（实体 PBR 渲染）----
    Shader m_shader;
    RenderList m_renderList; // built once per frame, shared by all mesh passes

    // ---- 渲染子系统（各自持有 GL 资源）----
    ShadowRenderer m_shadow;      // Pass0：阴影深度
    PostProcessor m_post;         // Pass2/3：Bloom + 后处理
    SkyRenderer m_sky;            // 天空盒
    ParticleRenderer m_particles; // 粒子
    GizmoRenderer m_gizmo;        // 网格/箭头/拖动

    bool m_showGizmo = true;          // 网格+坐标轴可见（Play 模式隐藏）
    bool m_playMode = false;          // Play 模式标志（相机选择策略用）
    bool m_showSky = true;            // 是否画天空盒
    bool m_postEnabled = true;        // 后处理总开关
    EnvironmentComponent m_env;       // 本帧环境参数（从场景解析，无则默认）
    flecs::entity_t m_previewCam = 0; // 预览相机实体（0=不显示小窗）

    // 拾取用：paintGL 每帧存下 view/proj（鼠标点击时逆变换用）
    glm::mat4 m_lastView{1.0f};
    glm::mat4 m_lastProj{1.0f};
};

#endif
