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
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    // 加载炸裂着色器（含几何着色器）
    m_explodeShader.load(SHADER_DIR "scene.vert", SHADER_DIR "explode.geom", SHADER_DIR "scene.frag");
    // 加载法线可视化着色器
    m_normalShader.load(SHADER_DIR "scene.vert", SHADER_DIR "normal_viz.geom", SHADER_DIR "normal_viz.frag");

    // ---- 立方体（位置+法线）----
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
    glBindVertexArray(0);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.05f, 0.06f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / height(), 0.1f, 100.0f);

    QVector3D lightPos(3.0f, 5.0f, 4.0f);
    QMatrix4x4 model;
    model.rotate(m_time * 30.0f, 0, 1, 0); // 旋转

    glBindVertexArray(m_cubeVAO);

    if (m_mode == 1)
    {
        // 模式1：法线可视化
        m_normalShader.use();
        m_normalShader.setMat4("uView", view.data());
        m_normalShader.setMat4("uProjection", projection.data());
        m_normalShader.setFloat("uNormalLen", 0.25f);
        // 顶点着色器需要 uModel 传递位置
        // scene.vert 用 uModel(位置) + uView + uProjection
        m_normalShader.setMat4("uModel", model.data());
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    else
    {
        // 模式0=炸裂, 模式2=普通（uExplode=false）
        m_explodeShader.use();
        m_explodeShader.setMat4("uModel", model.data());
        m_explodeShader.setMat4("uView", view.data());
        m_explodeShader.setMat4("uProjection", projection.data());
        m_explodeShader.setVec3("uLightPos", lightPos.x(), lightPos.y(), lightPos.z());
        m_explodeShader.setVec3("uViewPos", camPos.x(), camPos.y(), camPos.z());
        m_explodeShader.setVec3("uColor", 0.3f, 0.5f, 0.9f);
        m_explodeShader.setFloat("uTime", m_time);
        m_explodeShader.setFloat("uExplodeFactor", m_explodeFactor);
        m_explodeShader.setInt("uExplode", m_mode == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
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
        QPoint d = e->pos() - m_lastMouse;
        m_yaw += d.x() * 0.5f;
        m_pitch -= d.y() * 0.5f;
        m_pitch = qBound(-85.0f, m_pitch, 85.0f);
        m_lastMouse = e->pos();
    }
}

void OpenGLWidget::wheelEvent(QWheelEvent *e)
{
    if (m_mode == 0)
        m_explodeFactor = qBound(0.05f, m_explodeFactor + e->angleDelta().y() * 0.001f, 1.5f);
    else
        m_dist = qBound(2.0f, m_dist - e->angleDelta().y() * 0.01f, 30.0f);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_1)
        m_mode = 0; // 炸裂
    else if (e->key() == Qt::Key_2)
        m_mode = 1; // 法线可视化
    else if (e->key() == Qt::Key_3)
        m_mode = 2; // 普通
    else if (e->key() == Qt::Key_Escape)
        close();
}
