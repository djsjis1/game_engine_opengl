#include "openglwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent) { setFocusPolicy(Qt::StrongFocus); }
OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_cubeVAO)
        glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_cubeVBO)
        glDeleteBuffers(1, &m_cubeVBO);
    if (m_quadVAO)
        glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO)
        glDeleteBuffers(1, &m_quadVBO);
    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTexture)
        glDeleteTextures(1, &m_fboTexture);
    if (m_fboRBO)
        glDeleteRenderbuffers(1, &m_fboRBO);
    doneCurrent();
}

void OpenGLWidget::setupFBO(int w, int h)
{
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_fboTexture);
        glDeleteRenderbuffers(1, &m_fboRBO);
    }
    m_fboW = w;
    m_fboH = h;

    // 创建颜色附件纹理
    glGenTextures(1, &m_fboTexture);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 创建深度-模板渲染缓冲
    glGenRenderbuffers(1, &m_fboRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_fboRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

    // 创建 FBO 并绑定附件
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_fboRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        qDebug() << "FBO incomplete!";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_sceneShader.load(SHADER_DIR "scene.vert", SHADER_DIR "scene.frag");
    m_postShader.load(SHADER_DIR "screen.vert", SHADER_DIR "screen_post.frag");

    // 立方体顶点（6面×6顶点=36顶点，位置+法线）
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
    glBindVertexArray(m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    int cs = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, cs, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, cs, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 全屏四边形（NDC 的 2 个三角形 = xy + uv）
    float quadVerts[] = {
        -1, -1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1,
        -1, -1, 0, 0, 1, 1, 1, 1, -1, 1, 0, 1};
    glGenVertexArrays(1, &m_quadVAO);
    glBindVertexArray(m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    int qs = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, qs, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, qs, (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    setupFBO(width(), height());

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
    qDebug() << "Lesson 14 - FBO+PostProcessing: 1原图 2反相 3灰度 4锐化 5模糊 6边缘";
}

void OpenGLWidget::resizeGL(int w, int h)
{
    setupFBO(w, h);
    glViewport(0, 0, w, h);
}

void OpenGLWidget::renderScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fboW, m_fboH);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    m_sceneShader.use();
    QMatrix4x4 view, proj;
    view.lookAt(QVector3D(0, 2, 8), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    proj.perspective(45.0f, float(m_fboW) / float(m_fboH), 0.1f, 100.0f);
    m_sceneShader.setMat4("uView", view.constData());
    m_sceneShader.setMat4("uProjection", proj.constData());
    m_sceneShader.setVec3("uViewPos", 0, 2, 8);
    m_sceneShader.setVec3("uLightDir", -0.2f, -1.0f, -0.3f);

    QVector3D colors[] = {{1, 0.5, 0.3}, {0.3, 1, 0.5}, {0.5, 0.5, 1}, {1, 1, 0.3}, {1, 0.3, 0.8}};
    float offsets[][3] = {{0, 0, 0}, {2, 2, -3}, {-2, -1, -4}, {3, -2, -2}, {-3, 1, -5}, {0, -3, -6}, {4, 0, -4}, {-4, -1, -3}};

    glBindVertexArray(m_cubeVAO);
    for (int i = 0; i < 8; ++i)
    {
        QMatrix4x4 model;
        model.translate(offsets[i][0], offsets[i][1], offsets[i][2]);
        model.rotate(i * 30.0f + m_time * 25.0f, 0.3f, 1.0f, 0.0f);
        m_sceneShader.setMat4("uModel", model.constData());
        m_sceneShader.setVec3("uObjectColor", colors[i % 5].x(), colors[i % 5].y(), colors[i % 5].z());
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLWidget::renderPostProcessing()
{
    glViewport(0, 0, width(), height());
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_postShader.use();
    m_postShader.setInt("uEffect", m_effect);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    m_postShader.setInt("uScreenTexture", 0);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void OpenGLWidget::paintGL()
{
    renderScene();
    renderPostProcessing();
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    const char *names[] = {"原图", "反相", "灰度", "锐化", "模糊", "边缘检测"};
    switch (event->key())
    {
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
        m_effect = event->key() - Qt::Key_1;
        qDebug() << "Effect:" << names[m_effect];
        break;
    case Qt::Key_Escape:
        close();
        break;
    }
}
