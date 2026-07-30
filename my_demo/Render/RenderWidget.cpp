#include "Render/RenderWidget.h"
#include "Core/Raycast.h" // 射线拾取：屏幕→射线 + 立方体求交
#include <glm/gtc/type_ptr.hpp>
#include <QMouseEvent>
#include <QDebug>
#include <algorithm>
#include <vector>
#include "Core/GamePaths.h" // 运行时 shader/资源路径（导出/开发双模式）

RenderWidget::RenderWidget(QWidget *parent) : QOpenGLWidget(parent) {}

void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    // Shader 路径运行时解析：导出包用 exe 旁 assets/Shaders/，开发用 SHADER_DIR
    const std::string sd = GamePaths::shaderDir().toStdString();
    if (!m_shader.load(sd + "default.vert", sd + "default.frag"))
        qWarning() << "Shader load failed";
    if (!m_lineShader.load(sd + "line.vert", sd + "line.frag"))
        qWarning() << "Line shader load failed";
    initGizmo();
}

// ============================================================
// Godot 风格视口参考线：网格地面 + 原点坐标轴
//
// 顶点布局（分段存入同一个 VBO，drawGizmo 分批上色绘制）：
//   [0 .. m_gridVerts)          灰色网格线（XZ 平面，每 1m 一格）
//   [m_gridVerts+0 .. +2)       X 轴（红，±L 穿过原点）
//   [m_gridVerts+2 .. +4)       Y 轴（绿，向上短线 —— Godot 习惯）
//   [m_gridVerts+4 .. +6)       Z 轴（蓝，±L）
// ============================================================
void RenderWidget::initGizmo()
{
    const int kHalf = 20; // 网格半径：±20 米
    const float kL = (float)kHalf;

    std::vector<float> v; // 每顶点 3 float（位置）
    auto line = [&v](float x1, float y1, float z1, float x2, float y2, float z2)
    {
        v.insert(v.end(), {x1, y1, z1, x2, y2, z2});
    };

    // ① 网格线（跳过 0：原点处由彩色轴线代替，Godot 同款）
    for (int i = -kHalf; i <= kHalf; ++i)
    {
        if (i == 0)
            continue;
        line((float)i, 0, -kL, (float)i, 0, kL); // 沿 Z 方向
        line(-kL, 0, (float)i, kL, 0, (float)i); // 沿 X 方向
    }
    m_gridVerts = (int)(v.size() / 3);

    // ② 彩色坐标轴（X红 Y绿 Z蓝 —— 图形学通用记法 RGB=XYZ）
    line(-kL, 0, 0, kL, 0, 0); // X
    line(0, 0, 0, 0, 5, 0);    // Y（短线向上，认方向用）
    line(0, 0, -kL, 0, 0, kL); // Z

    glGenVertexArrays(1, &m_gizmoVao);
    glGenBuffers(1, &m_gizmoVbo);
    glBindVertexArray(m_gizmoVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gizmoVbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void RenderWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void RenderWidget::drawGizmo(const glm::mat4 &view, const glm::mat4 &proj)
{
    if (!m_showGizmo)
        return;
    m_lineShader.use();
    m_lineShader.setMat4("uView", view);
    m_lineShader.setMat4("uProjection", proj);
    m_lineShader.setMat4("uModel", glm::mat4(1));
    glBindVertexArray(m_gizmoVao);

    // ① 灰色网格（Godot 地面色调）
    m_lineShader.setVec3("uColor", 0.32f, 0.34f, 0.38f);
    glDrawArrays(GL_LINES, 0, m_gridVerts);

    // ② 彩色轴：X红 / Y绿 / Z蓝（各 2 顶点，紧跟在网格后）
    m_lineShader.setVec3("uColor", 0.86f, 0.26f, 0.30f); // X 红
    glDrawArrays(GL_LINES, m_gridVerts + 0, 2);
    m_lineShader.setVec3("uColor", 0.35f, 0.75f, 0.30f); // Y 绿
    glDrawArrays(GL_LINES, m_gridVerts + 2, 2);
    m_lineShader.setVec3("uColor", 0.30f, 0.45f, 0.90f); // Z 蓝
    glDrawArrays(GL_LINES, m_gridVerts + 4, 2);

    glBindVertexArray(0);
}

void RenderWidget::paintGL()
{
    // 执行延迟删除：这里天然有 GL 上下文（ResourceManager 的待删队列）
    ResourceManager::instance().flushPendingDeletes();

    // Godot 风格深灰蓝背景（突出网格与实体）
    glClearColor(0.11f, 0.125f, 0.155f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_world || !m_shader.isValid())
        return;

    // 从 ECS 查找 active 的 CameraComponent
    glm::mat4 view = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), float(width()) / std::max(height(), 1), 0.1f, 100.f);
    glm::vec3 cameraPos = glm::vec3(0, 3, 8);

    // 相机选择：编辑器相机（isEditor）优先——编辑器里场景可能带游戏相机，
    // 不能让它抢走视角；游戏运行时（GameWindow）没有 editor 相机，
    // 自然落到游戏相机——同一段代码两种环境都对
    bool foundEditor = false;
    m_world->each([&](flecs::entity, const CameraComponent &cam)
                  {
        if (!cam.active) return;
        if (foundEditor && !cam.isEditor) return; // 已有编辑器相机，游戏相机不覆盖
        if (cam.isEditor) foundEditor = true;
        view = cam.viewMatrix;
        cameraPos = cam.position;
        // 用实际窗口宽高比重算投影矩阵（CameraSystem 用的是默认值）
        proj = glm::perspective(glm::radians(cam.fov), float(width()) / std::max(height(), 1),
                                cam.nearPlane, cam.farPlane); });

    // 存下本帧矩阵：鼠标拾取时做逆变换（屏幕→世界射线）用
    m_lastView = view;
    m_lastProj = proj;

    drawGizmo(view, proj);

    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setVec3("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
    m_shader.setVec3("uLight.direction", -0.3f, -0.7f, -0.5f);
    m_shader.setVec3("uLight.ambient", 0.2f, 0.2f, 0.2f);
    m_shader.setVec3("uLight.diffuse", 0.8f, 0.8f, 0.8f);
    m_shader.setVec3("uLight.specular", 1.f, 1.f, 1.f);

    m_world->each([this](flecs::entity e, TransformComponent &tf, const MeshComponent &ms)
                  {
        if (!ms.isValid()) return;
        if (!tf.visibleInTree) return; // 隐藏的实体不渲染（含父隐藏）
        // worldMatrix 由 TransformSystem 每帧计算（含父子层级变换）
        m_shader.setMat4("uModel", tf.worldMatrix);
        m_shader.setMat3("uNormalMatrix", tf.worldNormalMatrix);
        const auto *mat = static_cast<const MaterialComponent*>(
            ecs_get_id(m_world->c_ptr(), e.raw_id(), m_world->component<MaterialComponent>().raw_id()));
        if (mat) {
            m_shader.setVec3("uMaterial.ambient", mat->ambient);
            m_shader.setVec3("uMaterial.diffuse", mat->diffuse);
            m_shader.setVec3("uMaterial.specular", mat->specular);
            m_shader.setFloat("uMaterial.shininess", mat->shininess);
        } else {
            m_shader.setVec3("uMaterial.ambient", 0.2f,0.2f,0.2f);
            m_shader.setVec3("uMaterial.diffuse", 0.7f,0.7f,0.7f);
            m_shader.setVec3("uMaterial.specular", 1,1,1);
            m_shader.setFloat("uMaterial.shininess", 32);
        }
        // 精灵帧纹理（图片动画）：有 SpriteComponent 且目录已登记 → 绑当前帧
        // 惰性 acquire：folder 已填但句柄未取（如刚加载场景）时在此登记
        bool textured = false;
        auto *sp = static_cast<SpriteComponent *>(const_cast<void *>(
            ecs_get_id(m_world->c_ptr(), e.raw_id(), m_world->component<SpriteComponent>().raw_id())));
        if (sp && sp->folder[0]) {
            if (!sp->spriteHandle)
                sp->spriteHandle = ResourceManager::instance().acquireSprite(sp->folder);
            const GpuTexture *tex =
                ResourceManager::instance().getSpriteFrame(sp->spriteHandle, sp->frame);
            if (tex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex->id);
                m_shader.setInt("uTexture", 0);
                textured = true;
            }
        }
        m_shader.setInt("uHasTexture", textured ? 1 : 0);

        // 句柄 → GPU 数据（多实体共享同一份，ResourceManager 缓存）
        const GpuMesh *gm = ResourceManager::instance().get(ms.meshHandle);
        if (!gm) return; // 资源已释放/无效句柄 → 安全跳过
        glBindVertexArray(gm->vao);
        if (gm->hasIndices) glDrawElements(gm->primitiveType, gm->indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(gm->primitiveType, 0, gm->vertexCount);
        glBindVertexArray(0); });
}

void RenderWidget::cleanupGL()
{
    makeCurrent();
    // 释放全部共享网格资源（ResourceManager 统一管理，不再逐实体释放）
    ResourceManager::instance().releaseAll();
    if (m_gizmoVao)
    {
        glDeleteVertexArrays(1, &m_gizmoVao);
        glDeleteBuffers(1, &m_gizmoVbo);
    }
    m_shader.clear();
    m_lineShader.clear();
    doneCurrent();
}

// ============================================================
// 射线拾取：左键点击视口 → 选中实体（对标 Godot/Unity 编辑器点选）
//
// 流程：像素坐标 → screenToRay 逆投影成世界射线
//   → 遍历可见实体（有 Mesh+Transform）逐个求交 → 取最近命中
//   → 发 entityPicked 信号（点空白发 0 = 取消选择）
// 右键不参与（留给相机操控）
// ============================================================
void RenderWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_world)
    {
        // ① 屏幕 → 世界射线（用本帧矩阵；高 DPI 下 devicePixelRatio 无影响，
        //    因为 width()/height() 与鼠标坐标同为逻辑像素）
        glm::mat4 invVP = glm::inverse(m_lastProj * m_lastView);
        Ray ray = screenToRay((float)event->pos().x(), (float)event->pos().y(),
                              (float)width(), (float)height(), invVP);

        // ② 遍历候选：有网格才能被点中；隐藏的跳过（与渲染一致）
        flecs::entity_t best = 0;
        float bestDist = 1e30f;
        m_world->each([&](flecs::entity e, TransformComponent &tf, const MeshComponent &)
                      {
            if (!tf.visibleInTree) return;
            float d;
            if (rayIntersectUnitCube(ray, tf.worldMatrix, d) && d < bestDist)
            {
                bestDist = d;
                best = e.raw_id();
            } });

        // ③ 通知外界（解耦：不直接碰编辑器面板）
        Q_EMIT entityPicked((quint64)best);
    }
    QOpenGLWidget::mousePressEvent(event); // 不吃掉事件（右键相机等照常）
}
