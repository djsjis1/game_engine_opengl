#include "openglwidget.h"
#include <QKeyEvent>
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
    m_materialShader.load(SHADER_DIR "material.vert", SHADER_DIR "material.frag");
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

    // 6种材质：翡翠、玉石、黄金、白银、青铜、珍珠
    m_materials = {
        {{0.0215f, 0.1745f, 0.0215f}, {0.07568f, 0.61424f, 0.07568f}, {0.633f, 0.727811f, 0.633f}, 76.8f},
        {{0.135f, 0.2225f, 0.1575f}, {0.54f, 0.89f, 0.63f}, {0.316228f, 0.316228f, 0.316228f}, 12.8f},
        {{0.24725f, 0.1995f, 0.0745f}, {0.75164f, 0.60648f, 0.22648f}, {0.628281f, 0.555802f, 0.366065f}, 51.2f},
        {{0.19225f, 0.19225f, 0.19225f}, {0.50754f, 0.50754f, 0.50754f}, {0.508273f, 0.508273f, 0.508273f}, 51.2f},
        {{0.2125f, 0.1275f, 0.054f}, {0.714f, 0.4284f, 0.18144f}, {0.393548f, 0.271906f, 0.166721f}, 25.6f},
        {{0.25f, 0.20725f, 0.20725f}, {1.0f, 0.829f, 0.829f}, {0.296648f, 0.296648f, 0.296648f}, 11.264f},
    };

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::setMaterial(const MaterialData &mat)
{
    m_materialShader.setVec3("uMaterial.ambient", mat.ambient.x(), mat.ambient.y(), mat.ambient.z());
    m_materialShader.setVec3("uMaterial.diffuse", mat.diffuse.x(), mat.diffuse.y(), mat.diffuse.z());
    m_materialShader.setVec3("uMaterial.specular", mat.specular.x(), mat.specular.y(), mat.specular.z());
    m_materialShader.setFloat("uMaterial.shininess", mat.shininess);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float radius = 5.0f;
    QVector3D lightPos(cos(m_time * 0.8f) * radius, 2.0f, sin(m_time * 0.8f) * radius);

    QMatrix4x4 view, projection;
    view.lookAt(QVector3D(0, 0, 12), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    m_materialShader.use();
    m_materialShader.setVec3("uLight.position", lightPos.x(), lightPos.y(), lightPos.z());
    m_materialShader.setVec3("uLight.ambient", 0.2f, 0.2f, 0.2f);
    m_materialShader.setVec3("uLight.diffuse", 0.8f, 0.8f, 0.8f);
    m_materialShader.setVec3("uLight.specular", 1.0f, 1.0f, 1.0f);
    m_materialShader.setVec3("uViewPos", 0, 0, 12);
    m_materialShader.setMat4("uView", view.constData());
    m_materialShader.setMat4("uProjection", projection.constData());

    glBindVertexArray(m_cubeVAO);
    for (int i = 0; i < m_materials.size(); ++i)
    {
        setMaterial(m_materials[i]);
        QMatrix4x4 model;
        model.translate(-7.5f + i * 3.0f, 0, 0);
        model.rotate(m_time * 30.0f + i * 15.0f, 0.5f, 1.0f, 0.0f);
        m_materialShader.setMat4("uModel", model.constData());
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    m_lightCubeShader.use();
    m_lightCubeShader.setMat4("uView", view.constData());
    m_lightCubeShader.setMat4("uProjection", projection.constData());
    QMatrix4x4 lightModel;
    lightModel.translate(lightPos);
    lightModel.scale(0.2f);
    m_lightCubeShader.setMat4("uModel", lightModel.constData());
    glBindVertexArray(m_lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
