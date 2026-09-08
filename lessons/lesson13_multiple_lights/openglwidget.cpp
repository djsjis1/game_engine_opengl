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
    if (m_lightVAO)
        glDeleteVertexArrays(1, &m_lightVAO);
    if (m_lightVBO)
        glDeleteBuffers(1, &m_lightVBO);
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_lightingShader.load(SHADER_DIR "multi_light.vert", SHADER_DIR "multi_light.frag");
    m_lightCubeShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    float vertices[] = {
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    int stride = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &m_lightVAO);
    glBindVertexArray(m_lightVAO);
    glGenBuffers(1, &m_lightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    m_pointLightPositions = {
        {0.7f, 0.2f, 2.0f}, {2.3f, -3.3f, -4.0f}, {-4.0f, 2.0f, -3.0f}, {0.0f, 0.0f, -3.0f}};
    m_pointLightColors = {
        {1.0f, 0.2f, 0.2f}, {0.2f, 1.0f, 0.2f}, {0.2f, 0.2f, 1.0f}, {1.0f, 1.0f, 0.3f}};
    m_cubePositions = {
        {0, 0, 0}, {2, 5, -15}, {-1.5f, -2.2f, -2.5f}, {-3.8f, -2, -12.3f}, {2.4f, -0.4f, -3.5f}, {-1.7f, 3, -7.5f}, {1.3f, -2, -2.5f}, {1.5f, 2, -5.5f}, {1.5f, 0.2f, -1.5f}, {-1.3f, 1, -1.5f}};

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
    qDebug() << "Lesson 13 - Multiple Lights: 1 Directional + 4 Point Lights";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    view.lookAt(QVector3D(0, 2, 8), QVector3D(0, 0, -3), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    m_lightingShader.use();
    m_lightingShader.setVec3("uViewPos", 0, 2, 8);
    m_lightingShader.setMat4("uView", view.constData());
    m_lightingShader.setMat4("uProjection", projection.constData());

    // 方向光
    m_lightingShader.setVec3("uDirLight.direction", -0.2f, -1.0f, -0.3f);
    m_lightingShader.setVec3("uDirLight.ambient", 0.05f, 0.05f, 0.05f);
    m_lightingShader.setVec3("uDirLight.diffuse", 0.3f, 0.3f, 0.3f);
    m_lightingShader.setVec3("uDirLight.specular", 0.4f, 0.4f, 0.4f);

    // 4个旋转的点光源
    for (int i = 0; i < m_pointLightPositions.size(); ++i)
    {
        QVector3D base = m_pointLightPositions[i];
        float radius = 2.5f + i * 0.5f;
        float speed = 0.3f + i * 0.15f;
        QVector3D pos(cos(m_time * speed + i) * radius, base.y(), sin(m_time * speed + i) * radius);
        QString idx = QString("uPointLights[%1].").arg(i);
        m_lightingShader.setVec3(idx + "position", pos.x(), pos.y(), pos.z());
        m_lightingShader.setVec3(idx + "ambient", 0.02f, 0.02f, 0.02f);
        m_lightingShader.setVec3(idx + "diffuse", m_pointLightColors[i].x(), m_pointLightColors[i].y(), m_pointLightColors[i].z());
        m_lightingShader.setVec3(idx + "specular", 1.0f, 1.0f, 1.0f);
        m_lightingShader.setFloat(idx + "constant", 1.0f);
        m_lightingShader.setFloat(idx + "linear", 0.09f);
        m_lightingShader.setFloat(idx + "quadratic", 0.032f);
    }

    // 绘制10个立方体
    glBindVertexArray(m_cubeVAO);
    for (int i = 0; i < m_cubePositions.size(); ++i)
    {
        QMatrix4x4 model;
        model.translate(m_cubePositions[i]);
        model.rotate(20.0f * i + m_time * 15.0f, 0.3f, 1.0f, 0.0f);
        m_lightingShader.setMat4("uModel", model.constData());
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 绘制光源小球
    m_lightCubeShader.use();
    m_lightCubeShader.setMat4("uView", view.constData());
    m_lightCubeShader.setMat4("uProjection", projection.constData());
    for (int i = 0; i < m_pointLightPositions.size(); ++i)
    {
        QVector3D base = m_pointLightPositions[i];
        float radius = 2.5f + i * 0.5f;
        float speed = 0.3f + i * 0.15f;
        QVector3D pos(cos(m_time * speed + i) * radius, base.y(), sin(m_time * speed + i) * radius);
        QMatrix4x4 lm;
        lm.translate(pos);
        lm.scale(0.15f);
        m_lightCubeShader.setMat4("uModel", lm.constData());
        glBindVertexArray(m_lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
