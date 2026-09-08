#ifndef GIZMORENDERER_H
#define GIZMORENDERER_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#include "Render/Shader.h"
#include "Core/Raycast.h"
#include <string>

// ============================================================
// GizmoRenderer — 编辑器视口辅助子系统（从 RenderWidget 拆出，单一职责）
//
// 职责：
//   ├─ drawGrid()：Godot 风格网格地面 + RGB 坐标轴（编辑模式参考线）
//   ├─ drawTransform()：选中实体的变换箭头（移动/旋转/缩放，屏幕恒定大小）
//   └─ 鼠标交互：射线-轴命中判定 + 拖动（handlePress/Move/Release）
//
// 拖动结果通过 DragResult 返回给调用方（RenderWidget 负责发信号/入撤销栈）。
// ============================================================
class GizmoRenderer : protected QOpenGLFunctions_3_3_Core
{
public:
    /// 拖动结束的结果（调用方包装成撤销命令 + 发信号）
    struct DragResult
    {
        bool committed = false;     // 是否发生了拖动
        flecs::entity_t target = 0; // 被拖动的实体
        int mode = 0;               // 0=移动 1=旋转 2=缩放
        glm::vec3 oldVal{0};        // 拖动前被编辑的值
        glm::vec3 newVal{0};        // 拖动后
    };

    void init(const std::string &shaderDir);
    void cleanup();

    /// 网格 + 坐标轴（visible=false 时跳过，如 Play 模式）
    void drawGrid(const glm::mat4 &view, const glm::mat4 &proj, bool visible);
    /// 变换箭头（target=0 时不画）
    void drawTransform(flecs::world &world, const glm::mat4 &view, const glm::mat4 &proj);

    // ---- 鼠标交互（返回 true = 事件被 gizmo 消费）----
    bool handlePress(flecs::world &world, const Ray &ray, const glm::mat4 &view);
    bool handleMove(flecs::world &world, const Ray &ray);
    DragResult handleRelease(flecs::world &world);

    // ---- 状态设置（编辑器同步）----
    void setTarget(flecs::entity_t id) { m_target = id; }
    void setMode(int mode) { m_mode = mode; }
    bool isDragging() const { return m_dragAxis >= 0; }

    /// 射线 vs 轴最近点参数（命中判定/拖动投影共用）
    static void closestRayAxis(const glm::vec3 &rayO, const glm::vec3 &rayD,
                               const glm::vec3 &axisO, const glm::vec3 &axisD,
                               float &tRay, float &tAxis);

private:
    // ---- Gizmo 尺寸常量（对标 Godot 编辑器 settings）----
    // Godot gizmo_size 默认 0.25 = 屏幕短边 1/4；本实现用“相机距离 × 系数”
    // 的等价近似（透视投影下两者数学同构：屏幕上恒定大小）。
    static constexpr float kGizmoScreenScale = 0.25f;  // 轴长 = 相机距离 × 此值
    static constexpr float kGizmoHitTolerance = 0.12f; // 命中容差 = 轴长 × 此值
    static constexpr float kGizmoMinLen = 0.3f;        // 极近距离轴长下限（防点不中）

    Shader m_lineShader;
    GLuint m_gridVao = 0, m_gridVbo = 0; // 网格 + 坐标轴
    GLuint m_axisVao = 0, m_axisVbo = 0; // 变换箭头（每帧动态上传）
    int m_gridVerts = 0;                 // 网格线顶点数（彩色轴紧随其后）

    flecs::entity_t m_target = 0;   // 选中实体（0=不显示箭头）
    int m_mode = 0;                 // 0=移动 1=旋转 2=缩放
    int m_dragAxis = -1;            // 拖动中的轴（0/1/2，-1=没拖）
    float m_dragStartAxisT = 0.0f;  // 按下时鼠标射线在轴上的投影参数
    glm::vec3 m_dragOrigin{0.0f};   // ★ 按下时冻结的 gizmo 原点（整个拖动期间不变）
                                    //   （若每帧从 worldMatrix 重读：原点随物体移动，
                                    //    delta 被周期性重置 → 物体回弹抖动，实踩 bug）
    glm::vec3 m_dragPlaneN{0.0f};   // ★ 拖动平面法线（按下时确定，冻结）：
                                    //   射线与平面求交 → 轴上投影 = delta。
                                    //   （旧的“射线-轴最近点”法对鼠标移动非线性，
                                    //    斜视角下橡皮筋手感，反人类）
    glm::vec3 m_dragStartVal{0.0f}; // 按下时被编辑的值
};

#endif
