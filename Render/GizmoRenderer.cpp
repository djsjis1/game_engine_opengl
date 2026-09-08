#include "Render/GizmoRenderer.h"
#include "Components/TransformComponent.h"
#include "Systems/PhysicsSystem.h" // 拖动同步物理体
#include <cmath>
#include <vector>
#include <QDebug>

void GizmoRenderer::init(const std::string &shaderDir)
{
    initializeOpenGLFunctions();
    if (!m_lineShader.load(shaderDir + "line.vert", shaderDir + "line.frag"))
        qWarning() << "Line shader load failed";

    // ============================================================
    // 网格 + 坐标轴：分段存入同一个 VBO，drawGrid 分批上色
    //   [0 .. m_gridVerts)         灰色网格线（XZ 平面，每 1m 一格）
    //   [m_gridVerts+0 .. +2)      X 轴（红）
    //   [m_gridVerts+2 .. +4)      Y 轴（绿）
    //   [m_gridVerts+4 .. +6)      Z 轴（蓝）
    // ============================================================
    const int kHalf = 20; // 网格半径：±20 米
    const float kL = (float)kHalf;
    std::vector<float> v; // 每顶点 3 float（位置）
    auto line = [&v](float x1, float y1, float z1, float x2, float y2, float z2)
    {
        v.insert(v.end(), {x1, y1, z1, x2, y2, z2});
    };
    for (int i = -kHalf; i <= kHalf; ++i)
    {
        if (i == 0)
            continue; // 原点处由彩色轴代替
        line((float)i, 0, -kL, (float)i, 0, kL);
        line(-kL, 0, (float)i, kL, 0, (float)i);
    }
    m_gridVerts = (int)(v.size() / 3);
    line(-kL, 0, 0, kL, 0, 0); // X
    line(0, 0, 0, 0, 5, 0);    // Y（短线向上）
    line(0, 0, -kL, 0, 0, kL); // Z

    glGenVertexArrays(1, &m_gridVao);
    glGenBuffers(1, &m_gridVbo);
    glBindVertexArray(m_gridVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void GizmoRenderer::cleanup()
{
    if (m_gridVao)
    {
        glDeleteVertexArrays(1, &m_gridVao);
        glDeleteBuffers(1, &m_gridVbo);
    }
    if (m_axisVao)
    {
        glDeleteVertexArrays(1, &m_axisVao);
        glDeleteBuffers(1, &m_axisVbo);
    }
    m_lineShader.clear();
}

void GizmoRenderer::drawGrid(const glm::mat4 &view, const glm::mat4 &proj, bool visible)
{
    if (!visible)
        return;
    m_lineShader.use();
    m_lineShader.setMat4("uView", view);
    m_lineShader.setMat4("uProjection", proj);
    m_lineShader.setMat4("uModel", glm::mat4(1));
    glBindVertexArray(m_gridVao);

    // ① 灰色网格（Godot 地面色调）
    m_lineShader.setVec3("uColor", 0.32f, 0.34f, 0.38f);
    glDrawArrays(GL_LINES, 0, m_gridVerts);
    // ② 彩色轴：X红 / Y绿 / Z蓝
    m_lineShader.setVec3("uColor", 0.86f, 0.26f, 0.30f);
    glDrawArrays(GL_LINES, m_gridVerts + 0, 2);
    m_lineShader.setVec3("uColor", 0.35f, 0.75f, 0.30f);
    glDrawArrays(GL_LINES, m_gridVerts + 2, 2);
    m_lineShader.setVec3("uColor", 0.30f, 0.45f, 0.90f);
    glDrawArrays(GL_LINES, m_gridVerts + 4, 2);
    glBindVertexArray(0);
}

// 三条粗轴线从实体位置伸出（X红 Y绿 Z蓝，拖动中的轴高亮黄）。
// 轴长随相机距离缩放 → 屏幕上恒定大小（近大远小会没法点）。
void GizmoRenderer::drawTransform(flecs::world &world, const glm::mat4 &view, const glm::mat4 &proj)
{
    if (!m_target || !m_lineShader.isValid())
        return;
    flecs::entity e(world.c_ptr(), m_target);
    if (!e.is_alive())
    {
        m_target = 0;
        return;
    }
    auto *tf = (const TransformComponent *)ecs_get_id(
        world.c_ptr(), m_target,
        world.component<TransformComponent>().raw_id());
    if (!tf)
        return;

    glm::vec3 origin(tf->worldMatrix[3]); // 世界位置（含父级）
    // 轴长 = 相机距离 × kGizmoScreenScale → 屏幕上恒定大小（Godot 同款：
    // gizmo_size=0.25 时屏幕短边 1/4，相机远近距离都保持可点手感）
    glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
    float len = glm::length(camPos - origin) * kGizmoScreenScale;
    if (len < 0.05f)
        len = kGizmoMinLen; // 缩放模式缩到 0 后仍有最小轴长可拖回

    // 惰性建 VBO（6 顶点动态上传）
    if (!m_axisVao)
    {
        glGenVertexArrays(1, &m_axisVao);
        glGenBuffers(1, &m_axisVbo);
        glBindVertexArray(m_axisVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_axisVbo);
        glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    const float v[18] = {
        origin.x,
        origin.y,
        origin.z,
        origin.x + len,
        origin.y,
        origin.z, // X
        origin.x,
        origin.y,
        origin.z,
        origin.x,
        origin.y + len,
        origin.z, // Y
        origin.x,
        origin.y,
        origin.z,
        origin.x,
        origin.y,
        origin.z + len, // Z
    };
    glBindBuffer(GL_ARRAY_BUFFER, m_axisVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);

    m_lineShader.use();
    m_lineShader.setMat4("uView", view);
    m_lineShader.setMat4("uProjection", proj);
    m_lineShader.setMat4("uModel", glm::mat4(1));

    glDisable(GL_DEPTH_TEST); // 箭头永远可见（被墙挡住也能拖，Godot 同款）
    glLineWidth(3.0f);
    glBindVertexArray(m_axisVao);
    const glm::vec3 axisColor[3] = {{0.86f, 0.26f, 0.30f},  // X 红
                                    {0.35f, 0.75f, 0.30f},  // Y 绿
                                    {0.30f, 0.45f, 0.90f}}; // Z 蓝
    for (int i = 0; i < 3; ++i)
    {
        glm::vec3 c = (i == m_dragAxis) ? glm::vec3(1.0f, 0.9f, 0.2f) // 拖动高亮黄
                                        : axisColor[i];
        m_lineShader.setVec3("uColor", c.x, c.y, c.z);
        glDrawArrays(GL_LINES, i * 2, 2);
    }
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

// 两条直线最近点的标准解（rayD/axisD 均为单位向量）
void GizmoRenderer::closestRayAxis(const glm::vec3 &rayO, const glm::vec3 &rayD,
                                   const glm::vec3 &axisO, const glm::vec3 &axisD,
                                   float &tRay, float &tAxis)
{
    glm::vec3 w = rayO - axisO;
    float b = glm::dot(rayD, axisD);
    float d0 = glm::dot(rayD, w);
    float e = glm::dot(axisD, w);
    float denom = 1.0f - b * b; // 平行时 ≈0
    if (denom < 1e-6f)
    {
        tRay = 0.0f;
        tAxis = e;
        return;
    }
    tRay = (b * e - d0) / denom;
    tAxis = (e - b * d0) / denom;
}

bool GizmoRenderer::handlePress(flecs::world &world, const Ray &ray, const glm::mat4 &view)
{
    if (!m_target || !world.entity(m_target).is_alive())
        return false;
    auto *tf = (const TransformComponent *)ecs_get_id(
        world.c_ptr(), m_target,
        world.component<TransformComponent>().raw_id());
    if (!tf)
        return false;

    glm::vec3 origin(tf->worldMatrix[3]);
    glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
    float len = glm::length(camPos - origin) * kGizmoScreenScale;
    const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float bestDist = len * kGizmoHitTolerance; // 命中容差（随 gizmo 大小缩放，Godot 同款）
    int hitAxis = -1;
    float hitT = 0;
    for (int i = 0; i < 3; ++i)
    {
        float tRay, tAxis;
        closestRayAxis(ray.origin, ray.dir, origin, axes[i], tRay, tAxis);
        if (tAxis < 0.0f || tAxis > len || tRay < 0.0f)
            continue; // 轴段之外/射线反向
        float d = glm::length((origin + axes[i] * tAxis) -
                              (ray.origin + ray.dir * tRay));
        if (d < bestDist)
        {
            bestDist = d;
            hitAxis = i;
            hitT = tAxis;
        }
    }
    if (hitAxis < 0)
        return false; // 没点中轴 → 交给拾取

    m_dragAxis = hitAxis;
    m_dragStartAxisT = hitT;
    m_dragOrigin = origin; // ★ 冻结原点：整个拖动期间 delta 都相对这个原点算

    // ★ 拖动平面：包含被拖的轴、尽量面向相机（Unity Handles 同款）。
    //   法线 = 视线方向投影到“垂直于轴”的平面：n = camDir - axis·dot(camDir,axis)
    //   （旧公式 cross(axis, camDir) 在相机正对物体时与视线恰好垂直 →
    //   射线平行平面、求交分母为 0 → 拖拽直接失效，实踩 bug）
    glm::vec3 camDir = glm::normalize(camPos - origin);
    glm::vec3 planeN = camDir - axes[hitAxis] * glm::dot(camDir, axes[hitAxis]);
    float nl = glm::length(planeN);
    if (nl < 1e-4f)
        planeN = glm::vec3(0.0f, 1.0f, 0.0f); // 视线几乎平行轴：退化，该轴拖不出位移（几何上正确）
    else
        planeN /= nl;
    m_dragPlaneN = planeN;

    // 按下时射线与平面的交点 → 沿轴的投影 = 起始参数（重算，与拖动统一数学）
    float denom = glm::dot(ray.dir, m_dragPlaneN);
    if (std::fabs(denom) < 1e-6f)
        return false;
    float t = glm::dot(origin - ray.origin, m_dragPlaneN) / denom;
    glm::vec3 P = ray.origin + ray.dir * t;
    m_dragStartAxisT = glm::dot(P - origin, axes[hitAxis]);

    // 记录被编辑的值（按当前模式取 pos/rot/scale）
    m_dragStartVal = m_mode == 1   ? tf->rotation
                     : m_mode == 2 ? tf->scale
                                   : tf->position;
    return true; // 吃掉：不进入拾取
}

bool GizmoRenderer::handleMove(flecs::world &world, const Ray &ray)
{
    if (m_dragAxis < 0)
        return false;
    auto *tf = (TransformComponent *)ecs_get_mut_id(
        world.c_ptr(), m_target,
        world.component<TransformComponent>().raw_id());
    if (!tf)
        return false;

    const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    // ★ 射线与拖动平面求交 → 交点在轴上的投影 = 当前参数。
    //   对鼠标移动线性、与视角无关（Godot 同款）；旧“射线-轴最近点”法
    //   在斜视角下灵敏度忽大忽小 → 橡皮筋手感（反人类，实踩 bug）
    float denom = glm::dot(ray.dir, m_dragPlaneN);
    if (std::fabs(denom) < 1e-6f)
        return true; // 视线与平面平行（极端视角）：保持原位
    float t = glm::dot(m_dragOrigin - ray.origin, m_dragPlaneN) / denom;
    glm::vec3 P = ray.origin + ray.dir * t;
    float axisT = glm::dot(P - m_dragOrigin, axes[m_dragAxis]);
    float delta = axisT - m_dragStartAxisT;

    // 按模式把 delta 作用到对应分量
    glm::vec3 newVal = m_dragStartVal;
    if (m_mode == 1)
        newVal[m_dragAxis] = m_dragStartVal[m_dragAxis] + delta * 45.0f; // 旋转：度
    else
        newVal[m_dragAxis] = m_dragStartVal[m_dragAxis] + delta; // 移动/缩放

    if (m_mode == 1)
        tf->rotation = newVal;
    else if (m_mode == 2)
        tf->scale = glm::max(newVal, glm::vec3(0.01f)); // 缩放不为负
    else
    {
        tf->position = newVal;
        physicsSetPosition(m_target, newVal.x, newVal.y, newVal.z);
    }

    // ★ 立即重算矩阵：worldMatrix 本要等下一帧 TransformSystem 才更新，
    //   拖动中 paintGL 读过期矩阵 → 渲染滞后于逻辑位置 → 闪烁。
    //   这里手动同步（无父实体时 worldMatrix=modelMatrix；有父的下一帧系统会修正）
    tf->updateMatrices();
    flecs::entity e(world.c_ptr(), m_target);
    flecs::entity parent = e.parent();
    if (!(parent && parent.has<TransformComponent>()))
        tf->worldMatrix = tf->modelMatrix;
    return true;
}

GizmoRenderer::DragResult GizmoRenderer::handleRelease(flecs::world &world)
{
    DragResult result;
    if (m_dragAxis < 0)
        return result;
    m_dragAxis = -1;

    auto *tf = (const TransformComponent *)ecs_get_id(
        world.c_ptr(), m_target,
        world.component<TransformComponent>().raw_id());
    if (!tf)
        return result;

    result.committed = true;
    result.target = m_target;
    result.mode = m_mode;
    result.oldVal = m_dragStartVal;
    result.newVal = m_mode == 1   ? tf->rotation
                    : m_mode == 2 ? tf->scale
                                  : tf->position;
    return result;
}
