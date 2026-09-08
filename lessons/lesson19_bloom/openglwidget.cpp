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
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteFramebuffers(1, &m_hdrFBO);
    glDeleteFramebuffers(1, &m_pingFBO[0]);
    glDeleteFramebuffers(1, &m_pingFBO[1]);
    glDeleteTextures(2, m_hdrTex);
    glDeleteTextures(2, m_pingTex);
    glDeleteRenderbuffers(1, &m_rboDepth);
    doneCurrent();
}

void OpenGLWidget::setupFBOs()
{
    m_fboW = width() * devicePixelRatio();
    m_fboH = height() * devicePixelRatio();
    if (m_fboW <= 0)
        m_fboW = 1;
    if (m_fboH <= 0)
        m_fboH = 1;

    // ---- HDR FBO: 两个颜色附件 ----
    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);

    glGenTextures(2, m_hdrTex);
    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, m_hdrTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_fboW, m_fboH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_hdrTex[i], 0);
    }

    glGenRenderbuffers(1, &m_rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_fboW, m_fboH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rboDepth);

    GLuint attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---- 乒乓 FBO (分辨率减半加速模糊) ----
    GLsizei halfW = m_fboW / 2, halfH = m_fboH / 2;
    if (halfW < 1)
        halfW = 1;
    if (halfH < 1)
        halfH = 1;
    for (int i = 0; i < 2; ++i)
    {
        glGenFramebuffers(1, &m_pingFBO[i]);
        glGenTextures(1, &m_pingTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, halfW, halfH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingTex[i], 0);
    }
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    m_sceneShader.load(SHADER_DIR "scene.vert", SHADER_DIR "scene.frag");
    m_blurShader.load(SHADER_DIR "screen.vert", SHADER_DIR "blur.frag");
    m_compositeShader.load(SHADER_DIR "screen.vert", SHADER_DIR "composite.frag");

    // ---- 立方体 VAO (位置+法线) ----
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

    // ---- 全屏四边形 ----
    float quad[] = {
        -1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0,
        -1, 1, 0, 1, 1, -1, 1, 0, 1, 1, 1, 1};
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

    setupFBOs();
}

void OpenGLWidget::resizeGL(int w, int h)
{
    if (w > 0 && h > 0)
    {
        glDeleteFramebuffers(1, &m_hdrFBO);
        glDeleteFramebuffers(1, &m_pingFBO[0]);
        glDeleteFramebuffers(1, &m_pingFBO[1]);
        glDeleteTextures(2, m_hdrTex);
        glDeleteTextures(2, m_pingTex);
        glDeleteRenderbuffers(1, &m_rboDepth);
        setupFBOs();
    }
}

void OpenGLWidget::renderSceneToFBO()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_fboW, m_fboH);
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0, 1, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(m_fboW) / m_fboH, 0.1f, 100.0f);

    QVector3D lightPos(0, 4.0f, 3.0f);

    m_sceneShader.use();
    m_sceneShader.setVec3("uLightPos", lightPos.x(), lightPos.y(), lightPos.z());
    m_sceneShader.setVec3("uViewPos", camPos.x(), camPos.y(), camPos.z());
    m_sceneShader.setMat4("uView", view.data());
    m_sceneShader.setMat4("uProjection", projection.data());

    QMatrix4x4 model;
    // 4 个彩色立方体排列在圆上
    QVector3D colors[] = {{1, 0.2f, 0.2f}, {0.2f, 1, 0.2f}, {0.2f, 0.4f, 1}, {1, 0.8f, 0.1f}};
    float positions[][3] = {{-1.2f, 1, 0}, {1.2f, 1, 0}, {0, 1, -1.2f}, {0, 1, 1.2f}};
    for (int i = 0; i < 4; ++i)
    {
        m_sceneShader.setVec3("uColor", colors[i].x(), colors[i].y(), colors[i].z());
        model.setToIdentity();
        model.translate(positions[i][0], positions[i][1], positions[i][2]);
        model.rotate(m_time * 40.0f * (i + 1) * 0.5f, 0, 1, 0);
        m_sceneShader.setMat4("uModel", model.data());
        glBindVertexArray(m_cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::blurPass(GLuint srcTex, GLuint dstFBO)
{
    // 水平模糊: srcTex → pingFBO[0]
    GLsizei halfW = m_fboW / 2, halfH = m_fboH / 2;
    if (halfW < 1)
        halfW = 1;
    if (halfH < 1)
        halfH = 1;

    glBindFramebuffer(GL_FRAMEBUFFER, m_pingFBO[0]);
    glViewport(0, 0, halfW, halfH);
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurShader.use();
    m_blurShader.setInt("uHorizontal", 1); // 水平
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    m_blurShader.setInt("uImage", 0);
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 垂直模糊: pingFBO[0] tex → dstFBO
    glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurShader.setInt("uHorizontal", 0); // 垂直
    glBindTexture(GL_TEXTURE_2D, m_pingTex[0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::renderComposite()
{
    glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
    glClear(GL_COLOR_BUFFER_BIT);
    m_compositeShader.use();
    m_compositeShader.setFloat("uIntensity", m_bloomLevel * 0.5f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrTex[0]); // 场景纹理
    m_compositeShader.setInt("uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_pingTex[1]); // 模糊后纹理
    m_compositeShader.setInt("uBloom", 1);
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLWidget::paintGL()
{
    renderSceneToFBO();

    // 提取高亮区域：把 hdrTex[0] 的高亮部分模糊
    // 简化为直接模糊整个场景（效果类似 bloom）
    // 实际 bloom 应该用亮度阈值过滤，这里做近似效果
    blurPass(m_hdrTex[0], m_pingFBO[1]);

    renderComposite();
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
    if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_5)
        m_bloomLevel = e->key() - Qt::Key_0;
    else if (e->key() == Qt::Key_Escape)
        close();
}
