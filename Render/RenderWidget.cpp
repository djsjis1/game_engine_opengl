// ============================================================
// RenderWidget.cpp — 渲染编排者（对标 Godot RenderingServer + Viewport）
//
// 【职责】从 ECS 世界读数据（相机/光源/实体/环境），调度各子系统绘制。
//
// 【渲染管线（paintGL 每帧执行）】
//   Pass 0: 阴影深度（ShadowRenderer）
//     ├─ 方向光/聚光 → 2D 深度图 (2048×2048)
//     └─ 点光 → Cubemap 6面深度 (512×512×6)
//   Pass 1: 主场景 → HDR 离屏 FBO (RGBA16F)
//     ├─ 天空盒（SkyRenderer）
//     ├─ 网格 + 坐标轴 + 变换箭头（GizmoRenderer）
//     ├─ 场景实体（PBR Cook-Torrance + 多光源 + 阴影）← 本文件
//     └─ 粒子（ParticleRenderer，实例化 billboard）
//   Pass 2/3: Bloom + 后处理 → 屏幕（PostProcessor）
//   Pass 4: 相机预览小窗（编辑器选中相机时）← 本文件
//
// 【核心设计思想】
//   1. 数据驱动：不存场景数据，每帧从 flecs world 读取
//   2. 子系统解耦：每个渲染子系统持有自己的 GL 资源、单一职责
//   3. 编辑器 vs 游戏复用：同一个 paintGL，编辑模式多画网格/Gizmo
//   4. HDR 管线：场景渲染到 RGBA16F，最后色调映射压回可显示范围
//
// 【学习路线（由浅入深）】
//   GizmoRenderer → lesson01-02 / PostProcessor → lesson19 / ShadowRenderer → lesson17
// ============================================================
#include "Render/RenderWidget.h"
#include "Core/Raycast.h"       // 射线拾取：屏幕→射线 + 立方体求交
#include "Core/Input.h"         // 键盘事件直接进 Input
#include "Core/TestFramework.h" // 帧统计（Agent 排障；宏关闭时零开销）
#include <glm/gtc/type_ptr.hpp>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <algorithm>
#include <chrono>
#include <vector>
#include <string>           // std::to_string（光源 uniform 名拼接）
#include <cmath>            // std::cos（聚光锥角）
#include "Core/GamePaths.h" // 运行时 shader/资源路径（导出/开发双模式）

RenderWidget::RenderWidget(QWidget *parent) : QOpenGLWidget(parent) {}

void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    const std::string sd = GamePaths::shaderDir().toStdString();
    if (!m_shader.load(sd + "default.vert", sd + "default.frag"))
        qWarning() << "Shader load failed";

    // 各渲染子系统初始化（各自加载 shader + 创建 GL 资源）
    m_gizmo.init(sd);
    m_sky.init(sd);
    m_particles.init(sd);
    m_shadow.init(sd);
    m_post.init(sd);
}

void RenderWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

// ============================================================
// paintGL — 多 Pass 编排（每帧执行）
// ============================================================
void RenderWidget::paintGL()
{
#if ENABLE_ENGINE_TESTING
    const auto paintStarted = std::chrono::steady_clock::now();
    auto recordPaintTime = [&paintStarted]
    {
        const auto elapsed = std::chrono::steady_clock::now() - paintStarted;
        EngineTest::recordPaintCpuTime(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()));
    };
#endif
    // 帧统计归零（TestFramework；宏关闭时零开销）
    EngineTest::beginFrame();

    // 执行延迟删除：这里天然有 GL 上下文（ResourceManager 的待删队列）
    ResourceManager::instance().flushPendingDeletes();
    ResourceManager::instance().pumpUploads();

    if (!m_world || !m_shader.isValid())
    {
        glClearColor(0.11f, 0.125f, 0.155f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#if ENABLE_ENGINE_TESTING
        recordPaintTime();
#endif
        return;
    }

    // 环境参数：从场景取第一个 EnvironmentComponent，无则用默认（老场景观感不变）
    m_env = EnvironmentComponent{};
    m_world->each([&](const EnvironmentComponent &ec)
                  { m_env = ec; });

    // Snapshot ready meshes once for shadows, main view and camera preview.
    m_renderList.build(*m_world);

    // ★ Pass 0：阴影（方向光优先；无方向光时聚光灯投影）
    int shadowLightType = -1;
    glm::vec3 shadowLightPos(0);
    glm::mat4 lightSpace = m_shadow.renderDirSpot(*m_world, m_renderList,
                                                  shadowLightType, shadowLightPos);
    glm::vec3 pointShadowPos(0);
    float pointShadowFar = 20.0f;
    bool pointShadowOn = m_shadow.renderPoint(*m_world, m_renderList,
                                              pointShadowPos, pointShadowFar);

    // ★ Pass 1 准备：主场景渲染到离屏 FBO（后处理关闭时回退默认 FBO）
    m_post.ensureFbo(width(), height());
    bool postOn = m_postEnabled && m_post.ready();
    glBindFramebuffer(GL_FRAMEBUFFER, postOn ? m_post.sceneFbo() : defaultFramebufferObject());
    glViewport(0, 0, width(), height());
    glClearColor(0.11f, 0.125f, 0.155f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- 相机选择（Play/Edit 策略，修复"相机不跟人物"根因）----
    // Play 优先游戏相机（脚本 set_camera 驱动的）；Edit 优先编辑器相机
    glm::mat4 view = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), float(width()) / std::max(height(), 1), 0.1f, 100.f);
    glm::vec3 cameraPos = glm::vec3(0, 3, 8);
    bool foundPrimary = false, foundFallback = false;
    glm::mat4 fallbackView = view;
    glm::vec3 fallbackPos = cameraPos;
    float fallbackFov = 45.f, fallbackNear = 0.1f, fallbackFar = 100.f;
    m_world->each([&](flecs::entity, const CameraComponent &cam)
                  {
        if (!cam.active) return;
        bool isGameCam = !cam.isEditor;
        bool isPrimary = m_playMode ? isGameCam : cam.isEditor;
        if (isPrimary && !foundPrimary)
        {
            foundPrimary = true;
            view = cam.viewMatrix;
            cameraPos = cam.position;
            proj = glm::perspective(glm::radians(cam.fov),
                                    float(width()) / std::max(height(), 1),
                                    cam.nearPlane, cam.farPlane);
        }
        else if (!isPrimary && !foundFallback)
        {
            foundFallback = true;
            fallbackView = cam.viewMatrix;
            fallbackPos = cam.position;
            fallbackFov = cam.fov;
            fallbackNear = cam.nearPlane;
            fallbackFar = cam.farPlane;
        } });
    if (!foundPrimary && foundFallback)
    {
        view = fallbackView;
        cameraPos = fallbackPos;
        proj = glm::perspective(glm::radians(fallbackFov),
                                float(width()) / std::max(height(), 1),
                                fallbackNear, fallbackFar);
    }
    m_lastView = view; // 拾取逆变换用
    m_lastProj = proj;

    // ---- 天空 + 编辑器辅助 ----
    m_sky.draw(view, proj, m_env, m_showSky);
    m_gizmo.drawGrid(view, proj, m_showGizmo);
    m_gizmo.drawTransform(*m_world, view, proj);

    // ---- 场景实体 ----
    m_shader.use();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setVec3("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

    // 光照数据驱动：收集场景所有光源实体喂 shader 数组（对标 Godot 多光源）
    int lightIdx = 0;
    m_world->each([&](flecs::entity, const LightComponent &lc, const TransformComponent &tf)
                  {
        if (lightIdx >= 8) return; // 与 shader MAX_LIGHTS 一致
        std::string p = "uLights[" + std::to_string(lightIdx) + "].";
        m_shader.setInt((p + "type").c_str(), lc.type);
        m_shader.setVec3((p + "position").c_str(), tf.position.x, tf.position.y, tf.position.z);
        m_shader.setVec3((p + "direction").c_str(), lc.direction.x, lc.direction.y, lc.direction.z);
        m_shader.setVec3((p + "ambient").c_str(), lc.ambient.x, lc.ambient.y, lc.ambient.z);
        m_shader.setVec3((p + "diffuse").c_str(),
                         lc.diffuse.x * lc.intensity, lc.diffuse.y * lc.intensity, lc.diffuse.z * lc.intensity);
        m_shader.setVec3((p + "specular").c_str(),
                         lc.specular.x * lc.intensity, lc.specular.y * lc.intensity, lc.specular.z * lc.intensity);
        m_shader.setFloat((p + "range").c_str(), lc.range);
        m_shader.setFloat((p + "spotCos").c_str(), std::cos(glm::radians(lc.spotAngle)));
        ++lightIdx; });
    m_shader.setInt("uLightCount", lightIdx);

    // 方向光/聚光阴影：绑深度贴图到纹理单元 1
    bool shadowOn = m_shadow.dirSpotReady() && (lightSpace != glm::mat4(1));
    m_shader.setInt("uShadowEnabled", shadowOn ? 1 : 0);
    if (shadowOn)
    {
        m_shader.setMat4("uLightSpaceMatrix", lightSpace);
        m_shader.setInt("uShadowLightType", shadowLightType);
        m_shader.setVec3("uShadowLightPos", shadowLightPos.x, shadowLightPos.y, shadowLightPos.z);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_shadow.dirSpotTexture());
        m_shader.setInt("uShadowMap", 1);
        glActiveTexture(GL_TEXTURE0); // 复位
    }

    // 点光阴影：cubemap 绑单元 2
    m_shader.setInt("uPointShadowEnabled", pointShadowOn ? 1 : 0);
    if (pointShadowOn)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_shadow.pointCube());
        m_shader.setInt("uPointShadowMap", 2);
        m_shader.setVec3("uPointShadowLightPos", pointShadowPos.x, pointShadowPos.y, pointShadowPos.z);
        m_shader.setFloat("uPointShadowFar", pointShadowFar);
        glActiveTexture(GL_TEXTURE0);
    }

    // 距离雾：环境组件驱动
    m_shader.setVec3("uFogColor", m_env.fogColor.x, m_env.fogColor.y, m_env.fogColor.z);
    m_shader.setFloat("uFogDensity", m_env.fogDensity);

    // Keep ECS order for now; transparent materials may need depth ordering.
    const RenderFrustum mainFrustum = RenderFrustum::fromViewProjection(proj * view);
    GLuint boundTexture = 0;
    GLuint boundVao = 0;
    bool hasBoundMaterial = false;
    glm::vec3 boundAlbedo(0.0f);
    float boundMetallic = 0.0f;
    float boundRoughness = 0.0f;
    float boundAo = 0.0f;
    int boundHasTexture = -1;
    glActiveTexture(GL_TEXTURE0);
    m_shader.setInt("uTexture", 0);
    for (const RenderItem &item : m_renderList.items())
    {
        if (!mainFrustum.intersects(item.worldBounds)) continue;
        m_shader.setMat4("uModel", item.model);
        m_shader.setMat3("uNormalMatrix", item.normal);

        const auto *mat = static_cast<const MaterialComponent *>(
            ecs_get_id(m_world->c_ptr(), item.entity,
                       m_world->component<MaterialComponent>().raw_id()));
        const glm::vec3 albedo = mat ? mat->albedo : glm::vec3(0.7f);
        const float metallic = mat ? mat->metallic : 0.0f;
        const float roughness = mat ? mat->roughness : 0.5f;
        const float ao = mat ? mat->ao : 1.0f;
        if (!hasBoundMaterial || glm::any(glm::notEqual(albedo, boundAlbedo)) ||
            metallic != boundMetallic || roughness != boundRoughness || ao != boundAo)
        {
            m_shader.setVec3("uMaterial.albedo", albedo);
            m_shader.setFloat("uMaterial.metallic", metallic);
            m_shader.setFloat("uMaterial.roughness", roughness);
            m_shader.setFloat("uMaterial.ao", ao);
            boundAlbedo = albedo;
            boundMetallic = metallic;
            boundRoughness = roughness;
            boundAo = ao;
            hasBoundMaterial = true;
        }

        // 纹理：精灵帧动画优先，其次材质 albedo 贴图（共用 uTexture 单元 0）
        bool textured = false;
        auto *sp = static_cast<SpriteComponent *>(const_cast<void *>(
            ecs_get_id(m_world->c_ptr(), item.entity,
                       m_world->component<SpriteComponent>().raw_id())));
        if (sp && sp->folder[0])
        {
            if (!sp->spriteHandle)
                sp->spriteHandle = ResourceManager::instance().acquireSprite(sp->folder);
            const GpuTexture *tex = ResourceManager::instance().getSpriteFrame(sp->spriteHandle, sp->frame);
            if (tex)
            {
                if (boundTexture != tex->id)
                {
                    glBindTexture(GL_TEXTURE_2D, tex->id);
                    EngineTest::countTextureBind();
                    boundTexture = tex->id;
                }
                textured = true;
            }
        }
        if (!textured && mat && mat->albedoTex[0])
        {
            auto *mmat = const_cast<MaterialComponent *>(mat);
            if (!mmat->texHandle)
            {
                mmat->texHandle = ResourceManager::instance().acquireMaterialTexture(mmat->albedoTex);
            }
            const GpuTexture *tex = ResourceManager::instance().getTexture(mmat->texHandle);
            if (tex)
            {
                if (boundTexture != tex->id)
                {
                    glBindTexture(GL_TEXTURE_2D, tex->id);
                    EngineTest::countTextureBind();
                    boundTexture = tex->id;
                }
                textured = true;
            }
        }
        const int hasTexture = textured ? 1 : 0;
        if (hasTexture != boundHasTexture)
        {
            m_shader.setInt("uHasTexture", hasTexture);
            boundHasTexture = hasTexture;
        }

        const GpuMesh &gm = item.mesh;
        if (boundVao != gm.vao)
        {
            glBindVertexArray(gm.vao);
            boundVao = gm.vao;
        }
        if (gm.hasIndices) glDrawElements(gm.primitiveType, gm.indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(gm.primitiveType, 0, gm.vertexCount);
        EngineTest::countDrawCall();
    }
    glBindVertexArray(0);

    // ★ 粒子 Pass：不透明之后、后处理之前
    m_particles.draw(*m_world, view, proj);

    // ★ 后处理 Pass：Bloom + 色调映射上屏
    if (postOn)
        m_post.render(m_env, width(), height(), defaultFramebufferObject());

    // ★ 相机预览小窗
    drawCameraPreview();

    // Include editor camera preview in the complete paintGL CPU sample.
#if ENABLE_ENGINE_TESTING
    ++EngineTest::stats().frameCounter;
    recordPaintTime();
#endif
}

// ============================================================
// 相机预览小窗（对标 Godot 选中 Camera3D 的 preview）：右下角
// viewport+scissor，用预览相机重画场景（无阴影/后处理的简化渲染）
// ============================================================
void RenderWidget::drawCameraPreview()
{
    if (!m_previewCam || !m_world || !m_shader.isValid())
        return;
    flecs::entity e(m_world->c_ptr(), m_previewCam);
    if (!e.is_alive())
    {
        m_previewCam = 0;
        return;
    }
    auto *cam = (const CameraComponent *)ecs_get_id(
        m_world->c_ptr(), m_previewCam,
        m_world->component<CameraComponent>().raw_id());
    if (!cam)
        return; // 选中的不是相机实体

    // 右下角小窗（1/4 宽高，8px 边距）；scissor 限制 clear 范围
    int pw = width() / 4, ph = height() / 4;
    int px = width() - pw - 8, py = 8; // GL 原点在左下
    glViewport(px, py, pw, ph);
    glEnable(GL_SCISSOR_TEST);
    glScissor(px - 1, py - 1, pw + 2, ph + 2);
    glClearColor(0.9f, 0.9f, 0.9f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT); // 白边
    glScissor(px, py, pw, ph);
    glClearColor(0.11f, 0.125f, 0.155f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 pview = cam->viewMatrix;
    glm::mat4 pproj = glm::perspective(glm::radians(cam->fov),
                                       float(pw) / std::max(ph, 1),
                                       cam->nearPlane, cam->farPlane);
    m_sky.draw(pview, pproj, m_env, m_showSky);

    // 简化几何 pass：光照 uniform 沿用主 pass 刚设的值，只换相机
    m_shader.use();
    m_shader.setMat4("uView", pview);
    m_shader.setMat4("uProjection", pproj);
    m_shader.setVec3("uCameraPos", cam->position.x, cam->position.y, cam->position.z);
    m_shader.setInt("uShadowEnabled", 0);      // 预览不做阴影（省深度 pass）
    m_shader.setInt("uPointShadowEnabled", 0); // 关掉主 pass 残留的点光阴影开关（否则预览误采样 cubemap）
    m_shader.setInt("uHasTexture", 0);
    const RenderFrustum previewFrustum = RenderFrustum::fromViewProjection(pproj * pview);
    for (const RenderItem &item : m_renderList.items())
    {
        if (!previewFrustum.intersects(item.worldBounds)) continue;
        m_shader.setMat4("uModel", item.model);
        m_shader.setMat3("uNormalMatrix", item.normal);
        const auto *mat = static_cast<const MaterialComponent *>(
            ecs_get_id(m_world->c_ptr(), item.entity,
                       m_world->component<MaterialComponent>().raw_id()));
        m_shader.setVec3("uMaterial.albedo", mat ? mat->albedo : glm::vec3(0.7f));
        m_shader.setFloat("uMaterial.metallic", mat ? mat->metallic : 0.0f);
        m_shader.setFloat("uMaterial.roughness", mat ? mat->roughness : 0.5f);
        m_shader.setFloat("uMaterial.ao", mat ? mat->ao : 1.0f);
        const GpuMesh &gm = item.mesh;
        glBindVertexArray(gm.vao);
        if (gm.hasIndices) glDrawElements(gm.primitiveType, gm.indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(gm.primitiveType, 0, gm.vertexCount);
    }
    glBindVertexArray(0);

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width(), height()); // 复原
}

// ============================================================
// 事件处理：键盘进 Input；鼠标 = Gizmo 优先 → 否则射线拾取
// ============================================================
void RenderWidget::keyPressEvent(QKeyEvent *event)
{
    Input::instance().onKey(event->key(), true);
    event->accept();
}

void RenderWidget::keyReleaseEvent(QKeyEvent *event)
{
    Input::instance().onKey(event->key(), false);
    event->accept();
}

void RenderWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_world)
    {
        glm::mat4 invVP = glm::inverse(m_lastProj * m_lastView);
        Ray ray = screenToRay((float)event->pos().x(), (float)event->pos().y(),
                              (float)width(), (float)height(), invVP);

        // ① Gizmo 轴命中优先 → 开始拖动（不进入拾取）
        if (m_gizmo.handlePress(*m_world, ray, m_lastView))
            return;

        // ② 射线拾取：遍历可见实体求交，取最近
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
        Q_EMIT entityPicked((quint64)best);
    }
    QOpenGLWidget::mousePressEvent(event);
}

void RenderWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_gizmo.isDragging() && m_world && (event->buttons() & Qt::LeftButton))
    {
        glm::mat4 invVP = glm::inverse(m_lastProj * m_lastView);
        Ray ray = screenToRay((float)event->pos().x(), (float)event->pos().y(),
                              (float)width(), (float)height(), invVP);
        if (m_gizmo.handleMove(*m_world, ray))
        {
            update(); // 拖动跟手
            return;
        }
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void RenderWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_gizmo.isDragging() && event->button() == Qt::LeftButton)
    {
        GizmoRenderer::DragResult r = m_gizmo.handleRelease(*m_world);
        if (r.committed)
        {
            // 发信号：MainWindow 包成撤销命令 + 刷新 Inspector
            Q_EMIT gizmoTransformed((quint64)r.target, r.mode,
                                    r.oldVal.x, r.oldVal.y, r.oldVal.z,
                                    r.newVal.x, r.newVal.y, r.newVal.z);
        }
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void RenderWidget::cleanupGL()
{
    makeCurrent();
    ResourceManager::instance().releaseAll();
    m_gizmo.cleanup();
    m_sky.cleanup();
    m_particles.cleanup();
    m_shadow.cleanup();
    m_post.cleanup();
    m_shader.clear();
    doneCurrent();
}
