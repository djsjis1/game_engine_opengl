// ============================================================
// RenderWidget.cpp —— OpenGL 渲染管线
//
// 【角色】把 ECS 世界里的数据画到屏幕上
//
// 【三个关键方法】
//   initializeGL()  — 窗口首次显示时调用一次（加载 Shader）
//   resizeGL(w,h)   — 窗口大小变化时调用（更新投影矩阵）
//   paintGL()       — 每帧调用（清屏 → 查摄像机 → 遍历实体绘制）
//
// 【paintGL 数据流】
//   1. 清屏
//   2. 从 ECS 世界查找 CameraComponent → 取 viewMatrix, position
//   3. 设置全局 uniform（view, projection, 灯光）
//   4. 遍历所有 (Transform + Mesh) 实体：
//      → TransformComponent.getModelMatrix()  → uModel
//      → MaterialComponent                    → uMaterial
//      → glBindVertexArray → glDrawElements
//
// 【扩展指南】
//   - 加新 Shader uniform → 在 paintGL 里加 setVec3/setFloat
//   - 加新网格形状      → 在 MeshFactory 里加 createSphere()
//   - 加后处理效果      → 用 FBO(帧缓冲) + 第二个 Shader
// ============================================================

#include "RenderWidget.h"
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>
#include <algorithm>

#include "Components/CameraComponent.h"

RenderWidget::RenderWidget(QWidget *parent) : QOpenGLWidget(parent) {}

// ---- 初始化：窗口首次显示时调用一次 ----
void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST); // 开启深度测试（远处的物体被近处的遮挡）

    // 加载着色器。SHADER_DIR 是 CMake 注入的宏（编译时定义）
    if (!m_shader.load(SHADER_DIR "default.vert", SHADER_DIR "default.frag"))
    {
        qWarning() << "Shader load failed:" << SHADER_DIR;
        return;
    }

    // 默认摄像机位置：从 (0,3,8) 看向原点
    // 如果 ECS 里有 CameraComponent，paintGL 会用它的 viewMatrix 覆盖这个
    m_viewMatrix = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}

// ---- 窗口大小变化时更新投影矩阵 ----
void RenderWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_projMatrix = glm::perspective(
        glm::radians(45.f),        // FOV = 45 度
        float(w) / std::max(h, 1), // 宽高比
        0.1f,                      // 近平面
        100.f);                    // 远平面
}

// ---- 每帧渲染 ----
void RenderWidget::paintGL()
{
    // 1. 清屏
    glClearColor(0.15f, 0.15f, 0.2f, 1.f); // 深蓝灰底色
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!m_world || !m_shader.isValid())
        return;

    // 2. 查找摄像机
    //    先设置默认值，再遍历 ECS 查找 active 的 CameraComponent
    glm::mat4 view = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), float(width()) / std::max(height(), 1), 0.1f, 100.f);
    glm::vec3 cameraPos = glm::vec3(0, 3, 8);

    m_world->each([&](flecs::entity, const CameraComponent &cam)
                  {
        if (cam.active) { view = cam.viewMatrix; cameraPos = cam.position; } });
    // 【注意】不从 cam 取 projMatrix（CameraSystem 不更新它，永远是单位矩阵）

    // 3. 激活 Shader + 设置全局 uniform
    m_shader.use();
    m_shader.setMat4("uView", view);                                       // 观察矩阵
    m_shader.setMat4("uProjection", proj);                                 // 投影矩阵
    m_shader.setVec3("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z); // 镜面反射用

    // ---- 硬编码方向光（后续可改为 LightComponent） ----
    // 光照方向：从右前上方照向左下
    m_shader.setVec3("uLight.direction", -0.3f, -0.7f, -0.5f);
    m_shader.setVec3("uLight.ambient", 0.2f, 0.2f, 0.2f); // 环境光（暗处基础亮度）
    m_shader.setVec3("uLight.diffuse", 0.8f, 0.8f, 0.8f); // 漫反射（主色调）
    m_shader.setVec3("uLight.specular", 1.f, 1.f, 1.f);   // 镜面光（高光）

    // 4. 遍历所有可渲染实体 (Transform + Mesh)
    m_world->each([this](flecs::entity e, TransformComponent &tf, const MeshComponent &ms)
                  {
        if (!ms.isValid()) return;    // 跳过空网格（vao == 0）

        // 4a. 模型变换
        // getModelMatrix() = 缩放 → 旋转(Z→Y→X) → 平移
        m_shader.setMat4("uModel",        tf.getModelMatrix());
        m_shader.setMat3("uNormalMatrix", tf.getNormalMatrix());

        // 4b. 材质（可选组件，有就用，没有用默认灰色）
        // 可选组件获取（C API：e.get<T>() 在 MSVC 有 bug，用 ecs_get_id 代替）
        const auto *mat = static_cast<const MaterialComponent *>(
            ecs_get_id(m_world->c_ptr(), e.raw_id(),
                       m_world->component<MaterialComponent>().raw_id()));
        if (mat) {
            m_shader.setVec3("uMaterial.ambient",  mat->ambient);
            m_shader.setVec3("uMaterial.diffuse",  mat->diffuse);
            m_shader.setVec3("uMaterial.specular", mat->specular);
            m_shader.setFloat("uMaterial.shininess", mat->shininess);
        } else {
            // 默认灰色材质
            m_shader.setVec3("uMaterial.ambient",  0.2f, 0.2f, 0.2f);
            m_shader.setVec3("uMaterial.diffuse",  0.7f, 0.7f, 0.7f);
            m_shader.setVec3("uMaterial.specular", 1.f, 1.f, 1.f);
            m_shader.setFloat("uMaterial.shininess", 32.f);
        }

        // 4c. 绘制
        glBindVertexArray(ms.vao);
        if (ms.hasIndices)
            glDrawElements(ms.primitiveType, ms.indexCount, GL_UNSIGNED_INT, nullptr);
        else
            glDrawArrays(ms.primitiveType, 0, ms.vertexCount);
        glBindVertexArray(0); });
}

// ---- 清理 GPU 资源 ----
void RenderWidget::cleanupGL()
{
    makeCurrent(); // 确保 GL 上下文激活

    if (m_world)
    {
        // 遍历所有 MeshComponent，销毁其中的 VAO/VBO/EBO
        m_world->each([this](flecs::entity, const MeshComponent &ms)
                      {
            if (ms.vao) m_factory.destroyMesh(const_cast<MeshComponent &>(ms)); });
    }

    m_shader.clear();
    doneCurrent();
}
