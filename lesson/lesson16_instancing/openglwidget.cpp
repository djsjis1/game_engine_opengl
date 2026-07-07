#include "openglwidget.h"
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <cstdlib>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    srand(42); // 固定种子
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_rockVAO)
    {
        glDeleteVertexArrays(1, &m_rockVAO);
        glDeleteBuffers(1, &m_rockVBO);
        glDeleteBuffers(1, &m_instanceVBO);
    }
    if (m_planetVAO)
    {
        glDeleteVertexArrays(1, &m_planetVAO);
        glDeleteBuffers(1, &m_planetVBO);
    }
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_shader.load(SHADER_DIR "instanced.vert", SHADER_DIR "instanced.frag");

    // ---- 生成简易"小行星"网格（随机偏移立方体顶点） ----
    float baseVerts[] = {
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

    // 轻微偏移顶点使更像石头
    float rockVerts[36 * 6];
    for (int i = 0; i < 36 * 6; ++i)
    {
        float r = (float(rand()) / RAND_MAX - 0.5f) * 0.12f;
        rockVerts[i] = baseVerts[i] + r * 0.5f;
    }

    glGenVertexArrays(1, &m_rockVAO);
    glBindVertexArray(m_rockVAO);
    glGenBuffers(1, &m_rockVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_rockVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rockVerts), rockVerts, GL_STATIC_DRAW);
    int s = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, s, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, s, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---- 生成 1000 个随机变换矩阵 ----
    QVector<float> instances(INSTANCE_COUNT * 16);
    for (int i = 0; i < INSTANCE_COUNT; ++i)
    {
        // 环形分布
        float angle = float(i) / INSTANCE_COUNT * 360.0f * 3.14159f / 180.0f;
        float displacement = (rand() % 20) * 0.1f;
        float radius = 15.0f + displacement;
        float x = sin(angle) * radius + float(rand() % 100 - 50) * 0.1f;
        float y = displacement * 0.5f + float(rand() % 100 - 50) * 0.1f;
        float z = cos(angle) * radius + float(rand() % 100 - 50) * 0.1f;
        float scale = 0.1f + float(rand() % 100) * 0.005f;

        QMatrix4x4 m;
        m.translate(x, y, z);
        m.rotate(float(rand() % 360), float(rand() % 100) / 100.0f, float(rand() % 100) / 100.0f, float(rand() % 100) / 100.0f);
        m.scale(scale);
        // 把颜色塞进矩阵第2行的前三列（HACK: mat4用row[1]=颜色）
        float cr = 0.4f + float(rand() % 60) * 0.01f;
        float cg = 0.3f + float(rand() % 50) * 0.01f;
        float cb = 0.2f + float(rand() % 30) * 0.01f;
        float *md = m.data();
        md[4] = cr;
        md[5] = cg;
        md[6] = cb; // row[1]前三列存颜色
        memcpy(&instances[i * 16], m.constData(), 16 * sizeof(float));
    }

    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(float), instances.constData(), GL_STATIC_DRAW);
    // mat4 占 4 个 location (2,3,4,5)
    for (int j = 0; j < 4; ++j)
    {
        glVertexAttribPointer(2 + j, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void *)(j * 4 * sizeof(float)));
        glEnableVertexAttribArray(2 + j);
        glVertexAttribDivisor(2 + j, 1); // 关键：每个实例换一次
    }

    // ---- 中心大行星（半球体简化） ----
    glGenVertexArrays(1, &m_planetVAO);
    glBindVertexArray(m_planetVAO);
    glGenBuffers(1, &m_planetVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_planetVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(baseVerts), baseVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, s, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, s, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // 中心行星不用实例化，直接绘制
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time+=0.016f; update(); });
    m_timer->start(16);
    qDebug() << "Lesson 16 - Instancing:" << INSTANCE_COUNT << "颗小行星 鼠标旋转/滚轮缩放";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, proj;
    proj.perspective(45.0f, float(width()) / float(height()), 0.1f, 200.0f);
    QVector3D cam;
    cam.setX(m_dist * cos(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    cam.setY(m_dist * sin(qDegreesToRadians(m_pitch)));
    cam.setZ(m_dist * sin(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    view.lookAt(cam, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    m_shader.use();
    m_shader.setMat4("uView", view.constData());
    m_shader.setMat4("uProjection", proj.constData());
    m_shader.setVec3("uViewPos", cam.x(), cam.y(), cam.z());
    m_shader.setVec3("uLightDir", -0.3f, -1.0f, -0.2f);

    // 实例化绘制1000颗小行星
    glBindVertexArray(m_rockVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, INSTANCE_COUNT);

    // 中心大行星（也走实例化，只画1个）
    QMatrix4x4 planetM;
    planetM.rotate(m_time * 10.0f, 0.2f, 1.0f, 0.0f);
    planetM.scale(2.5f);
    // 把颜色塞进矩阵 row[1]
    float *pmd = planetM.data();
    pmd[4] = 0.2f;
    pmd[5] = 0.6f;
    pmd[6] = 1.0f;
    // 上传单实例矩阵，用 glDrawArraysInstanced(..., 1)
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 16 * sizeof(float), planetM.constData());
    glBindVertexArray(m_planetVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 1);

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
void OpenGLWidget::wheelEvent(QWheelEvent *e)
{
    m_dist -= e->angleDelta().y() * 0.1f;
    if (m_dist < 5)
        m_dist = 5;
    if (m_dist > 150)
        m_dist = 150;
}
void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
}
