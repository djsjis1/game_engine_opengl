#include "openglwidget.h"
#include <QtMath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
    setFocusPolicy(Qt::StrongFocus);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    glDeleteVertexArrays(1, &m_cubeVAO);
    glDeleteBuffers(1, &m_cubeVBO);
    glDeleteVertexArrays(1, &m_planeVAO);
    glDeleteBuffers(1, &m_planeVBO);
    glDeleteVertexArrays(1, &m_lightVAO);
    glDeleteBuffers(1, &m_lightVBO);
    glDeleteFramebuffers(1, &m_depthFBO);
    glDeleteTextures(1, &m_depthTex);
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    // ---- 加载着色器 ----
    m_shadowShader.load(SHADER_DIR "shadow_depth.vert", SHADER_DIR "shadow_depth.frag");
    m_sceneShader.load(SHADER_DIR "scene.vert", SHADER_DIR "scene.frag");
    m_lightShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    // ---- 立方体 VAO (位置+法线) ----
    float cubeVerts[] = {
        -0.5f, -0.5f, -0.5f, 0, 0, -1, 0.5f, -0.5f, -0.5f, 0, 0, -1, 0.5f, 0.5f, -0.5f, 0, 0, -1,
        0.5f, 0.5f, -0.5f, 0, 0, -1, -0.5f, 0.5f, -0.5f, 0, 0, -1, -0.5f, -0.5f, -0.5f, 0, 0, -1,
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 0, 1,
        0.5f, 0.5f, 0.5f, 0, 0, 1, -0.5f, 0.5f, 0.5f, 0, 0, 1, -0.5f, -0.5f, 0.5f, 0, 0, 1,
        -0.5f, 0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, -0.5f, -1, 0, 0, -0.5f, -0.5f, -0.5f, -1, 0, 0,
        -0.5f, -0.5f, -0.5f, -1, 0, 0, -0.5f, -0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0,
        0.5f, 0.5f, 0.5f, 1, 0, 0, 0.5f, 0.5f, -0.5f, 1, 0, 0, 0.5f, -0.5f, -0.5f, 1, 0, 0,
        0.5f, -0.5f, -0.5f, 1, 0, 0, 0.5f, -0.5f, 0.5f, 1, 0, 0, 0.5f, 0.5f, 0.5f, 1, 0, 0,
        -0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, 0.5f, 0, -1, 0,
        0.5f, -0.5f, 0.5f, 0, -1, 0, -0.5f, -0.5f, 0.5f, 0, -1, 0, -0.5f, -0.5f, -0.5f, 0, -1, 0,
        -0.5f, 0.5f, -0.5f, 0, 1, 0, 0.5f, 0.5f, -0.5f, 0, 1, 0, 0.5f, 0.5f, 0.5f, 0, 1, 0,
        0.5f, 0.5f, 0.5f, 0, 1, 0, -0.5f, 0.5f, 0.5f, 0, 1, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0};
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindVertexArray(m_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---- 地面 VAO (位置+法线) ----
    float planeVerts[] = {
        10, 0, 10, 0, 1, 0, -10, 0, 10, 0, 1, 0, -10, 0, -10, 0, 1, 0,
        10, 0, 10, 0, 1, 0, -10, 0, -10, 0, 1, 0, 10, 0, -10, 0, 1, 0};
    glGenVertexArrays(1, &m_planeVAO);
    glGenBuffers(1, &m_planeVBO);
    glBindVertexArray(m_planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVerts), planeVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---- 光源指示器 VAO ----
    glGenVertexArrays(1, &m_lightVAO);
    glGenBuffers(1, &m_lightVBO);
    glBindVertexArray(m_lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // ---- 创建深度贴图 FBO ----
    glGenFramebuffers(1, &m_depthFBO);
    glGenTextures(1, &m_depthTex);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE); // 深度Pass不需要颜色输出
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindVertexArray(0);
}

void OpenGLWidget::renderDepthMap()
{
    // 【知识点】从光源视角渲染场景，生成深度贴图
    QMatrix4x4 lightProjection, lightView, lightSpace;
    lightProjection.ortho(-10, 10, -10, 10, 0.1f, 30.0f);

    // 光源绕场景上方旋转
    float lx = 5.0f * cosf(m_time * 0.5f);
    float lz = 5.0f * sinf(m_time * 0.5f);
    QVector3D lightPos(lx, 8.0f, lz);
    QVector3D lightTarget(0, 0, 0);
    lightView.lookAt(lightPos, lightTarget, QVector3D(0, 1, 0));
    lightSpace = lightProjection * lightView;

    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_shadowShader.use();
    m_shadowShader.setMat4("uLightSpace", lightSpace.data());

    // 画地面
    QMatrix4x4 model;
    m_shadowShader.setMat4("uLightSpace", (lightSpace * model).data());
    glBindVertexArray(m_planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 画 2 个大立方体
    model.setToIdentity();
    model.translate(-1.5f, 1.0f, 0);
    m_shadowShader.setMat4("uLightSpace", (lightSpace * model).data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    model.setToIdentity();
    model.translate(2.0f, 1.5f, -1.5f);
    m_shadowShader.setMat4("uLightSpace", (lightSpace * model).data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 画地上的小立方体
    model.setToIdentity();
    model.translate(0, 0.5f, 2.0f);
    m_shadowShader.setMat4("uLightSpace", (lightSpace * model).data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::renderScene()
{
    glClearColor(0.1f, 0.12f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 相机
    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0, 1.5f, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / height(), 0.1f, 100.0f);

    // 光源空间矩阵
    float lx = 5.0f * cosf(m_time * 0.5f);
    float lz = 5.0f * sinf(m_time * 0.5f);
    QVector3D lightPos(lx, 8.0f, lz);
    QMatrix4x4 lightProj, lightView, lightSpace;
    lightProj.ortho(-10, 10, -10, 10, 0.1f, 30.0f);
    lightView.lookAt(lightPos, QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    lightSpace = lightProj * lightView;

    // 绑定深度贴图为纹理单元 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);

    m_sceneShader.use();
    m_sceneShader.setInt("uShadowMap", 0);
    m_sceneShader.setMat4("uLightSpace", lightSpace.data());
    m_sceneShader.setMat4("uView", view.data());
    m_sceneShader.setMat4("uProjection", projection.data());
    m_sceneShader.setVec3("uViewPos", camPos.x(), camPos.y(), camPos.z());
    m_sceneShader.setVec3("uLightDir", -lx, -8.0f, -lz); // 方向光方向

    QMatrix4x4 model;

    // 画地面（灰色）
    m_sceneShader.setVec3("uObjectColor", 0.4f, 0.4f, 0.4f);
    model.setToIdentity();
    m_sceneShader.setMat4("uModel", model.data());
    glBindVertexArray(m_planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 画立方体 1（红色）
    m_sceneShader.setVec3("uObjectColor", 0.8f, 0.3f, 0.3f);
    model.setToIdentity();
    model.translate(-1.5f, 1.0f, 0);
    m_sceneShader.setMat4("uModel", model.data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 画立方体 2（蓝色）
    m_sceneShader.setVec3("uObjectColor", 0.3f, 0.4f, 0.8f);
    model.setToIdentity();
    model.translate(2.0f, 1.5f, -1.5f);
    m_sceneShader.setMat4("uModel", model.data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 画小立方体 3（绿色）
    m_sceneShader.setVec3("uObjectColor", 0.3f, 0.7f, 0.4f);
    model.setToIdentity();
    model.translate(0, 0.5f, 2.0f);
    m_sceneShader.setMat4("uModel", model.data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 画光源位置指示器（小黄框）
    m_lightShader.use();
    m_lightShader.setMat4("uView", view.data());
    m_lightShader.setMat4("uProjection", projection.data());
    model.setToIdentity();
    model.translate(lightPos);
    model.scale(0.2f);
    m_lightShader.setMat4("uModel", model.data());
    glBindVertexArray(m_lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void OpenGLWidget::paintGL()
{
    renderDepthMap();
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    renderScene();
}

void OpenGLWidget::resizeGL(int w, int h) {}

void OpenGLWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_lastMouse = e->pos();
        m_rotating = true;
    }
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rotating)
    {
        QPoint delta = e->pos() - m_lastMouse;
        m_yaw += delta.x() * 0.5f;
        m_pitch -= delta.y() * 0.5f;
        m_pitch = qBound(-85.0f, m_pitch, 85.0f);
        m_lastMouse = e->pos();
    }
}

void OpenGLWidget::wheelEvent(QWheelEvent *e) { m_dist = qBound(3.0f, m_dist - e->angleDelta().y() * 0.01f, 50.0f); }

void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
}
