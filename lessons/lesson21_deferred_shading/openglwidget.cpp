#include "openglwidget.h"
#include <QtMath>
#include <QDebug>
#include <cmath>

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
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteVertexArrays(1, &m_sphereVAO);
    glDeleteBuffers(1, &m_sphereVBO);
    glDeleteFramebuffers(1, &m_gbufferFBO);
    glDeleteTextures(1, &m_gPosTex);
    glDeleteTextures(1, &m_gNormalTex);
    glDeleteTextures(1, &m_gAlbedoSpecTex);
    glDeleteRenderbuffers(1, &m_gDepthRBO);
    doneCurrent();
}

void OpenGLWidget::setupGBuffer()
{
    m_fboW = width() * devicePixelRatio();
    m_fboH = height() * devicePixelRatio();
    if (m_fboW <= 0)
        m_fboW = 1;
    if (m_fboH <= 0)
        m_fboH = 1;

    glGenFramebuffers(1, &m_gbufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gbufferFBO);

    // 附件1: 世界坐标(RGB16F)
    glGenTextures(1, &m_gPosTex);
    glBindTexture(GL_TEXTURE_2D, m_gPosTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_fboW, m_fboH, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gPosTex, 0);

    // 附件2: 法线(RGB16F)
    glGenTextures(1, &m_gNormalTex);
    glBindTexture(GL_TEXTURE_2D, m_gNormalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_fboW, m_fboH, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gNormalTex, 0);

    // 附件3: 颜色+镜面(每个RGBA)
    glGenTextures(1, &m_gAlbedoSpecTex);
    glBindTexture(GL_TEXTURE_2D, m_gAlbedoSpecTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fboW, m_fboH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gAlbedoSpecTex, 0);

    // 深度渲染缓冲
    glGenRenderbuffers(1, &m_gDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_fboW, m_fboH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_gDepthRBO);

    GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        qDebug() << "FBO incomplete! Status:" << fboStatus << "Size:" << m_fboW << m_fboH;
    else
        qDebug() << "G-Buffer FBO OK, size:" << m_fboW << m_fboH;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 生成近似球体（UV球算法）
static void genSphere(QVector<float> &verts)
{
    const int stacks = 20, slices = 20;
    for (int i = 0; i < stacks; ++i)
    {
        float phi0 = M_PI * float(i) / stacks;
        float phi1 = M_PI * float(i + 1) / stacks;
        for (int j = 0; j < slices; ++j)
        {
            float theta0 = 2.0f * M_PI * float(j) / slices;
            float theta1 = 2.0f * M_PI * float(j + 1) / slices;

            auto p = [](float phi, float theta) -> QVector3D
            {
                return {sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta)};
            };
            QVector3D p00 = p(phi0, theta0), p01 = p(phi0, theta1);
            QVector3D p10 = p(phi1, theta0), p11 = p(phi1, theta1);

            auto add = [&](const QVector3D &v, const QVector3D &n)
            {
                verts.push_back(v.x());
                verts.push_back(v.y());
                verts.push_back(v.z());
                verts.push_back(n.x());
                verts.push_back(n.y());
                verts.push_back(n.z());
            };
            add(p00, p00);
            add(p01, p01);
            add(p10, p10);
            add(p10, p10);
            add(p01, p01);
            add(p11, p11);
        }
    }
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    if (!m_gbufferShader.load(SHADER_DIR "gbuffer.vert", SHADER_DIR "gbuffer.frag"))
        qDebug() << "FATAL: G-Buffer shader load failed!";
    else
        qDebug() << "G-Buffer shader OK, program:" << m_gbufferShader.program();
    if (!m_lightShader.load(SHADER_DIR "light_pass.vert", SHADER_DIR "light_pass.frag"))
        qDebug() << "FATAL: Light shader load failed!";
    else
        qDebug() << "Light shader OK, program:" << m_lightShader.program();

    // ---- 立方体 ----
    float cube[] = {
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---- 球体 ----
    QVector<float> sphereVerts;
    genSphere(sphereVerts);
    m_sphereCount = sphereVerts.size() / 6;
    glGenVertexArrays(1, &m_sphereVAO);
    glGenBuffers(1, &m_sphereVBO);
    glBindVertexArray(m_sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size() * sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---- 全屏四边形 ----
    float quad[] = {-1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, -1, 1, 0, 1, 1, 1, 1};
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    setupGBuffer();

    // 初始化10个光源
    for (int i = 0; i < 10; ++i)
    {
        float a = float(i) / 10.0f * 2.0f * M_PI;
        PointLight l;
        l.pos = QVector3D(cosf(a) * 3.0f, 1.5f + sinf(a * 3.0f) * 1.0f, sinf(a) * 3.0f);
        l.color = QVector3D(0.3f + 0.7f * cosf(a * 2.1f), 0.3f + 0.7f * sinf(a * 1.7f), 0.3f + 0.7f * cosf(a * 3.3f));
        l.color = l.color.normalized() * 0.8f + QVector3D(0.2f, 0.2f, 0.2f);
        m_lights.push_back(l);
    }
}

void OpenGLWidget::renderGeometryPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_gbufferFBO);
    // 显式确保 MRT 写入目标
    GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);
    glViewport(0, 0, m_fboW, m_fboH);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0, 0.5f, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(m_fboW) / m_fboH, 0.1f, 100.0f);

    m_gbufferShader.use();
    m_gbufferShader.setMat4("uView", view.data());
    m_gbufferShader.setMat4("uProjection", projection.data());

    QMatrix4x4 model;

    // 地面（大长方体）
    m_gbufferShader.setFloat("uMetallic", 0.1f);
    m_gbufferShader.setVec3("uColor", 0.5f, 0.5f, 0.5f);
    model.setToIdentity();
    model.translate(0, -1.5f, 0);
    model.scale(5, 0.1f, 5);
    m_gbufferShader.setMat4("uModel", model.data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 4个彩色立方体
    QVector3D cubeColors[] = {{0.8f, 0.2f, 0.2f}, {0.2f, 0.8f, 0.2f}, {0.2f, 0.4f, 0.9f}, {0.9f, 0.7f, 0.1f}};
    float cubePos[][3] = {{-1.2f, 0, -1.2f}, {1.2f, 0, -1.2f}, {-1.2f, 0, 1.2f}, {1.2f, 0, 1.2f}};
    for (int i = 0; i < 4; ++i)
    {
        m_gbufferShader.setFloat("uMetallic", 0.8f);
        m_gbufferShader.setVec3("uColor", cubeColors[i].x(), cubeColors[i].y(), cubeColors[i].z());
        model.setToIdentity();
        model.translate(cubePos[i][0], cubePos[i][1], cubePos[i][2]);
        model.rotate(m_time * 20.0f, 0, 1, 0);
        m_gbufferShader.setMat4("uModel", model.data());
        glBindVertexArray(m_cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 中心球体（金属）
    m_gbufferShader.setFloat("uMetallic", 1.0f);
    m_gbufferShader.setVec3("uColor", 0.9f, 0.6f, 0.2f);
    model.setToIdentity();
    model.translate(0, 0.2f, 0);
    m_gbufferShader.setMat4("uModel", model.data());
    glBindVertexArray(m_sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, m_sphereCount);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::renderLightPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_lightShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gPosTex);
    m_lightShader.setInt("gPosition", 0);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLWidget::paintGL()
{
    // 终极测试：直接画到屏幕，跳过G-Buffer，看几何体能否出现
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0, 0.5f, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / height(), 0.1f, 100.0f);
    qDebug() << "cam:" << camPos << "yaw:" << m_yaw << "pitch:" << m_pitch;

    m_gbufferShader.use();
    m_gbufferShader.setMat4("uView", view.data());
    m_gbufferShader.setMat4("uProjection", projection.data());

    // 只画一个立方体
    m_gbufferShader.setFloat("uMetallic", 0.0f);
    m_gbufferShader.setVec3("uColor", 1.0f, 0.0f, 0.0f);
    QMatrix4x4 model;
    m_gbufferShader.setMat4("uModel", model.data());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    GLenum err = glGetError();
    if (err)
        qDebug() << "GL error:" << err;
}

void OpenGLWidget::resizeGL(int w, int h)
{
    if (w > 0 && h > 0)
    {
        glDeleteFramebuffers(1, &m_gbufferFBO);
        glDeleteTextures(1, &m_gPosTex);
        glDeleteTextures(1, &m_gNormalTex);
        glDeleteTextures(1, &m_gAlbedoSpecTex);
        glDeleteRenderbuffers(1, &m_gDepthRBO);
        setupGBuffer();
    }
}

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
        QPoint d = e->pos() - m_lastMouse;
        m_yaw += d.x() * 0.5f;
        m_pitch -= d.y() * 0.5f;
        m_pitch = qBound(-85.0f, m_pitch, 85.0f);
        m_lastMouse = e->pos();
    }
}

void OpenGLWidget::wheelEvent(QWheelEvent *e) { m_dist = qBound(3.0f, m_dist - e->angleDelta().y() * 0.01f, 30.0f); }

void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
}
