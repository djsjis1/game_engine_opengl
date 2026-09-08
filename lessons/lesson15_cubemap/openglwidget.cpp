#include "openglwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_cubeVAO)
        glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_cubeVBO)
        glDeleteBuffers(1, &m_cubeVBO);
    if (m_skyboxVAO)
        glDeleteVertexArrays(1, &m_skyboxVAO);
    if (m_skyboxVBO)
        glDeleteBuffers(1, &m_skyboxVBO);
    if (m_cubemapTex)
        glDeleteTextures(1, &m_cubemapTex);
    doneCurrent();
}

GLuint OpenGLWidget::loadCubemap()
{
    // 6面渐变：+X红, -X青, +Y蓝, -Y黄, +Z绿, -Z紫
    QVector<QPair<GLenum, QColor>> faces = {
        {GL_TEXTURE_CUBE_MAP_POSITIVE_X, QColor(230, 100, 100)}, // 右 - 红
        {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, QColor(80, 180, 200)},  // 左 - 青
        {GL_TEXTURE_CUBE_MAP_POSITIVE_Y, QColor(60, 140, 240)},  // 上 - 蓝/天
        {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, QColor(180, 160, 80)},  // 下 - 土黄
        {GL_TEXTURE_CUBE_MAP_POSITIVE_Z, QColor(100, 200, 100)}, // 前 - 绿
        {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, QColor(180, 100, 200)}, // 后 - 紫
    };

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    int size = 256;
    for (auto &f : faces)
    {
        QImage img(size, size, QImage::Format_RGB32);
        QColor base = f.second;
        for (int y = 0; y < size; ++y)
        {
            float t = float(y) / size; // 0(下) → 1(上)
            int r = int(base.red() * (0.3f + 0.7f * t));
            int g = int(base.green() * (0.3f + 0.7f * t));
            int b = int(base.blue() * (0.3f + 0.7f * t));
            for (int x = 0; x < size; ++x)
                img.setPixel(x, y, qRgb(r, g, b));
        }
        img = img.mirrored(); // OpenGL y轴翻转
        glTexImage2D(f.first, 0, GL_RGB, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_cubeShader.load(SHADER_DIR "reflective.vert", SHADER_DIR "reflective.frag");
    m_skyboxShader.load(SHADER_DIR "skybox.vert", SHADER_DIR "skybox.frag");

    m_cubemapTex = loadCubemap();

    // 立方体
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
    int s = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, s, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, s, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 天空盒（6面NDC立方体）
    float skyVerts[] = {
        -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
        -1, -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1,
        1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1,
        -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1, 1,
        -1, 1, -1, 1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1, 1, -1, 1, -1,
        -1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &m_skyboxVAO);
    glBindVertexArray(m_skyboxVAO);
    glGenBuffers(1, &m_skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyVerts), skyVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time+=0.016f; update(); });
    m_timer->start(16);
    qDebug() << "Lesson 15 - Cubemap: 左=反射 右=折射 鼠标拖动旋转";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, proj;
    proj.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);
    QVector3D cam;
    cam.setX(6.0f * cos(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    cam.setY(6.0f * sin(qDegreesToRadians(m_pitch)));
    cam.setZ(6.0f * sin(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    view.lookAt(cam, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // ---- 绘制天空盒（最后画，利用深度技巧） ----
    glDepthFunc(GL_LEQUAL);
    m_skyboxShader.use();
    m_skyboxShader.setMat4("uView", view.constData());
    m_skyboxShader.setMat4("uProjection", proj.constData());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
    m_skyboxShader.setInt("uSkybox", 0);
    glBindVertexArray(m_skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    // ---- 绘制 2 个立方体 ----
    m_cubeShader.use();
    m_cubeShader.setMat4("uView", view.constData());
    m_cubeShader.setMat4("uProjection", proj.constData());
    m_cubeShader.setVec3("uViewPos", cam.x(), cam.y(), cam.z());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTex);
    m_cubeShader.setInt("uSkybox", 0);
    glBindVertexArray(m_cubeVAO);

    // 左边：反射
    QMatrix4x4 m1;
    m1.translate(-1.5f, 0, 0);
    m_cubeShader.setMat4("uModel", m1.constData());
    m_cubeShader.setFloat("uReflect", 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 右边：折射
    QMatrix4x4 m2;
    m2.translate(1.5f, sin(m_time) * 0.5f, 0);
    m_cubeShader.setMat4("uModel", m2.constData());
    m_cubeShader.setFloat("uReflect", 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0);
}

void OpenGLWidget::mousePressEvent(QMouseEvent *e)
{
    m_lastMouse = e->pos();
    m_rotating = true;
}
void OpenGLWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_rotating)
        return;
    QPoint d = e->pos() - m_lastMouse;
    m_lastMouse = e->pos();
    m_yaw += d.x() * 0.3f;
    m_pitch -= d.y() * 0.3f;
    if (m_pitch > 89)
        m_pitch = 89;
    if (m_pitch < -89)
        m_pitch = -89;
}
void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
}
