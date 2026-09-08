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
    m_lightingShader.load(SHADER_DIR "light_casters.vert", SHADER_DIR "light_casters.frag");
    m_lightCubeShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    // 立方体顶点（位置 + 法线）
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

    // 10个立方体位置
    m_cubePositions = {
        {0, 0, 0}, {2, 5, -15}, {-1.5f, -2.2f, -2.5f}, {-3.8f, -2, -12.3f}, {2.4f, -0.4f, -3.5f}, {-1.7f, 3, -7.5f}, {1.3f, -2, -2.5f}, {1.5f, 2, -5.5f}, {1.5f, 0.2f, -1.5f}, {-1.3f, 1, -1.5f}};

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
    qDebug() << "Lesson 12 - Press 1=Directional  2=Point  3=Spot  Esc=Exit";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    view.lookAt(QVector3D(0, 3, 10), QVector3D(0, 0, -5), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    // ============ 根据开关设置光源参数 ============
    m_lightingShader.use();
    m_lightingShader.setInt("uLight.type", m_lightType);
    m_lightingShader.setVec3("uLight.ambient", 0.2f, 0.2f, 0.2f);
    m_lightingShader.setVec3("uLight.diffuse", 0.8f, 0.8f, 0.8f);
    m_lightingShader.setVec3("uLight.specular", 1.0f, 1.0f, 1.0f);
    m_lightingShader.setVec3("uViewPos", 0, 3, 10);
    m_lightingShader.setMat4("uView", view.constData());
    m_lightingShader.setMat4("uProjection", projection.constData());

    float radius = 5.0f;
    QVector3D lightPos(cos(m_time * 0.5f) * radius, 2.0f + sin(m_time * 0.3f) * 2.0f, sin(m_time * 0.5f) * radius);
    QVector3D lightDir(-0.2f, -1.0f, -0.3f);

    if (m_lightType == 0)
    {
        // 方向光：无位置，只看方向
        m_lightingShader.setVec3("uLight.direction", lightDir.x(), lightDir.y(), lightDir.z());
    }
    else
    {
        m_lightingShader.setVec3("uLight.position", lightPos.x(), lightPos.y(), lightPos.z());
        m_lightingShader.setVec3("uLight.direction", lightDir.x(), lightDir.y(), lightDir.z());
        m_lightingShader.setFloat("uLight.constant", 1.0f);
        m_lightingShader.setFloat("uLight.linear", 0.09f);
        m_lightingShader.setFloat("uLight.quadratic", 0.032f);
        if (m_lightType == 2)
        {
            m_lightingShader.setFloat("uLight.cutOff", cos(12.5f * 3.14159f / 180.0f));
            m_lightingShader.setFloat("uLight.outerCutOff", cos(17.5f * 3.14159f / 180.0f));
        }
    }

    // 绘制10个立方体
    glBindVertexArray(m_cubeVAO);
    for (int i = 0; i < m_cubePositions.size(); ++i)
    {
        QMatrix4x4 model;
        model.translate(m_cubePositions[i]);
        model.rotate(20.0f * i + m_time * 15.0f, 0.5f, 1.0f, 0.0f);
        m_lightingShader.setMat4("uModel", model.constData());
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 绘制光源小球（非方向光时显示位置）
    if (m_lightType != 0)
    {
        m_lightCubeShader.use();
        m_lightCubeShader.setMat4("uView", view.constData());
        m_lightCubeShader.setMat4("uProjection", projection.constData());
        QMatrix4x4 lm;
        lm.translate(lightPos);
        lm.scale(0.2f);
        m_lightCubeShader.setMat4("uModel", lm.constData());
        glBindVertexArray(m_lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_1:
        m_lightType = 0;
        qDebug() << ">> Directional Light (方向光)";
        break;
    case Qt::Key_2:
        m_lightType = 1;
        qDebug() << ">> Point Light (点光源)";
        break;
    case Qt::Key_3:
        m_lightType = 2;
        qDebug() << ">> Spot Light (聚光灯)";
        break;
    case Qt::Key_Escape:
        close();
        break;
    }
}
