#include "RenderWidget.h"
#include "System/EditorCamera.h"
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>
#include <algorithm>
#include "Components/CameraComponent.h"

RenderWidget::RenderWidget(QWidget *parent) : QOpenGLWidget(parent) {}

void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    if (!m_shader.load(SHADER_DIR "default.vert", SHADER_DIR "default.frag"))
        qWarning() << "Shader load failed";
    if (!m_lineShader.load(SHADER_DIR "line.vert", SHADER_DIR "line.frag"))
        qWarning() << "Line shader load failed";
    initGizmo();
}

void RenderWidget::initGizmo()
{
    float L = 100.0f;
    float lines[] = {
        0,
        0,
        0,
        L,
        0,
        0,
        0,
        0,
        0,
        0,
        L,
        0,
        0,
        0,
        0,
        0,
        0,
        L,
        0,
        0,
        0,
        -L,
        0,
        0,
        0,
        0,
        0,
        0,
        -L,
        0,
        0,
        0,
        0,
        0,
        0,
        -L,
    };
    glGenVertexArrays(1, &m_gizmoVao);
    glGenBuffers(1, &m_gizmoVbo);
    glBindVertexArray(m_gizmoVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gizmoVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
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
    glm::mat4 id(1.0f);
    m_lineShader.setMat4("uModel", id);

    glBindVertexArray(m_gizmoVao);
    // 正半轴（亮色）  负半轴（暗色）
    float bright[] = {1, 0, 0};
    m_lineShader.setVec3("uColor", bright[0], bright[1], bright[2]);
    glDrawArrays(GL_LINES, 0, 6); // 三条正半轴
    float dim[] = {0.4f, 0, 0};
    m_lineShader.setVec3("uColor", dim[0], dim[1], dim[2]);
    glDrawArrays(GL_LINES, 6, 6); // 三条负半轴
    glBindVertexArray(0);
}

void RenderWidget::paintGL()
{
    glClearColor(0.15f, 0.15f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_world || !m_shader.isValid())
        return;

    glm::mat4 view = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.f), float(width()) / std::max(height(), 1), 0.1f, 100.f);
    glm::vec3 cameraPos = glm::vec3(0, 3, 8);

    if (m_editorCam)
    {
        view = m_editorCam->viewMatrix;
        proj = m_editorCam->projMatrix;
        cameraPos = m_editorCam->position;
    }
    else
    {
        m_world->each([&](flecs::entity, const CameraComponent &cam)
                      {
            if (cam.active) { view = cam.viewMatrix; cameraPos = cam.position; } });
    }

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
        m_shader.setMat4("uModel", tf.getModelMatrix());
        m_shader.setMat3("uNormalMatrix", tf.getNormalMatrix());
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
        glBindVertexArray(ms.vao);
        if (ms.hasIndices) glDrawElements(ms.primitiveType, ms.indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(ms.primitiveType, 0, ms.vertexCount);
        glBindVertexArray(0); });
}

void RenderWidget::cleanupGL()
{
    makeCurrent();
    if (m_world)
    {
        m_world->each([this](flecs::entity, const MeshComponent &ms)
                      {
            if (ms.vao) m_factory.destroyMesh(const_cast<MeshComponent&>(ms)); });
    }
    if (m_gizmoVao)
    {
        glDeleteVertexArrays(1, &m_gizmoVao);
        glDeleteBuffers(1, &m_gizmoVbo);
    }
    m_shader.clear();
    m_lineShader.clear();
    doneCurrent();
}
