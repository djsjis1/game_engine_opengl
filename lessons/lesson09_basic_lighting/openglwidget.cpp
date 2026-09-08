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
    m_lightingShader.load(SHADER_DIR "lighting.vert", SHADER_DIR "lighting.frag");
    m_lightCubeShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    float vertices[] = {
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        0.5f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        0.5f,
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        0.5f,
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        -1.0f,
        0.0f,
        0.0f,
        -0.5f,
        -0.5f,
        0.5f,
        -1.0f,
        0.0f,
        0.0f,
        -0.5f,
        0.5f,
        0.5f,
        -1.0f,
        0.0f,
        0.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        -1.0f,
        0.0f,
        0.0f,
        -0.5f,
        0.5f,
        0.5f,
        -1.0f,
        0.0f,
        0.0f,
        -0.5f,
        0.5f,
        -0.5f,
        -1.0f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        -1.0f,
        0.0f,
        0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        -1.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        0.0f,
        -1.0f,
        0.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        -1.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        0.0f,
        -1.0f,
        0.0f,
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        -1.0f,
        0.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        0.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        0.0f,
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        0.0f,
    };

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

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float radius = 3.0f;
    QVector3D lightPos(cos(m_time) * radius, 1.5f, sin(m_time) * radius);
    QVector3D lightColor(1.0f, 1.0f, 1.0f);

    QMatrix4x4 view, projection;
    view.lookAt(QVector3D(0, 0, 6), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    m_lightingShader.use();
    m_lightingShader.setVec3("uObjectColor", 1.0f, 0.5f, 0.31f);
    m_lightingShader.setVec3("uLightColor", lightColor.x(), lightColor.y(), lightColor.z());
    m_lightingShader.setVec3("uLightPos", lightPos.x(), lightPos.y(), lightPos.z());
    m_lightingShader.setVec3("uViewPos", 0, 0, 6);
    QMatrix4x4 model;
    m_lightingShader.setMat4("uModel", model.constData());
    m_lightingShader.setMat4("uView", view.constData());
    m_lightingShader.setMat4("uProjection", projection.constData());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    m_lightCubeShader.use();
    m_lightCubeShader.setVec3("uLightColor", lightColor.x(), lightColor.y(), lightColor.z());
    model.setToIdentity();
    model.translate(lightPos);
    model.scale(0.2f);
    m_lightCubeShader.setMat4("uModel", model.constData());
    m_lightCubeShader.setMat4("uView", view.constData());
    m_lightCubeShader.setMat4("uProjection", projection.constData());
    glBindVertexArray(m_lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
