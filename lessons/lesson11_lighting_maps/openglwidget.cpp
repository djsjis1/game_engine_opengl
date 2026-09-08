#include "openglwidget.h"
#include <QKeyEvent>
#include <QPainter>
#include <QDebug>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

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
    if (m_diffuseMap)
        glDeleteTextures(1, &m_diffuseMap);
    if (m_specularMap)
        glDeleteTextures(1, &m_specularMap);
    doneCurrent();
}

// 程序化生成"木箱"风格的漫反射贴图
QImage OpenGLWidget::makeContainerImage(int size)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    img.fill(QColor(120, 75, 35)); // 木材底色
    QPainter p(&img);
    // 木纹
    for (int i = 0; i < 60; ++i)
    {
        int y = qrand() % size;
        p.setPen(QColor(90 + qrand() % 30, 55 + qrand() % 20, 25, 180));
        p.drawLine(0, y, size, y + (qrand() % 5 - 2));
    }
    // 金属边框
    int border = size / 16;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(80, 80, 90));
    p.drawRect(0, 0, size, border);
    p.drawRect(0, size - border, size, border);
    p.drawRect(0, 0, border, size);
    p.drawRect(size - border, 0, border, size);
    // 中心金属铆钉
    p.setBrush(QColor(140, 140, 150));
    int r = size / 12;
    p.drawEllipse(size / 2 - r / 2, size / 2 - r / 2, r, r);
    p.end();
    return img;
}

// 镜面贴图：木材区域=黑(无高光)，金属区域=白(有高光)
QImage OpenGLWidget::makeSpecularMaskImage(int size)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    img.fill(QColor(0, 0, 0)); // 默认无高光
    QPainter p(&img);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255));
    int border = size / 16;
    p.drawRect(0, 0, size, border);
    p.drawRect(0, size - border, size, border);
    p.drawRect(0, 0, border, size);
    p.drawRect(size - border, 0, border, size);
    int r = size / 12;
    p.drawEllipse(size / 2 - r / 2, size / 2 - r / 2, r, r);
    p.end();
    return img;
}

GLuint OpenGLWidget::createTexture(const QImage &image)
{
    QImage flipped = image.mirrored(false, true);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, flipped.width(), flipped.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.bits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_lightingShader.load(SHADER_DIR "lighting_maps.vert", SHADER_DIR "lighting_maps.frag");
    m_lightCubeShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    // 立方体顶点数据：位置(3) + 法线(3) + 纹理坐标(2)，共 8 个 float
    float vertices[] = {
        // back face (z = -0.5, normal -z)
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        // front face (z = 0.5, normal +z)
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        // left face (x = -0.5, normal -x)
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        // right face (x = 0.5, normal +x)
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        // bottom face (y = -0.5, normal -y)
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        // top face (y = 0.5, normal +y)
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};

    // cube VAO
    glGenVertexArrays(1, &m_cubeVAO);
    glBindVertexArray(m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    int stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // light VAO（只用位置）
    glGenVertexArrays(1, &m_lightVAO);
    glBindVertexArray(m_lightVAO);
    glGenBuffers(1, &m_lightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // 生成贴图
    m_diffuseMap = createTexture(makeContainerImage(256));
    m_specularMap = createTexture(makeSpecularMaskImage(256));

    glEnable(GL_DEPTH_TEST);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
        m_time += 0.016f;
        update(); });
    m_timer->start(16);

    qDebug() << "Lesson 11 - Lighting Maps: 木质表面无高光，金属边框有强烈高光";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 旋转的光源
    float radius = 2.5f;
    QVector3D lightPos(cos(m_time) * radius, sin(m_time * 0.7f) * 1.5f, sin(m_time) * radius);
    QVector3D viewPos(0, 0, 4);

    QMatrix4x4 view, projection;
    view.lookAt(viewPos, QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / float(height()), 0.1f, 100.0f);

    m_lightingShader.use();
    // 绑定贴图到纹理单元
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_diffuseMap);
    m_lightingShader.setInt("uMaterial.diffuse", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_specularMap);
    m_lightingShader.setInt("uMaterial.specular", 1);
    m_lightingShader.setFloat("uMaterial.shininess", 64.0f);

    m_lightingShader.setVec3("uLight.position", lightPos.x(), lightPos.y(), lightPos.z());
    m_lightingShader.setVec3("uLight.ambient", 0.2f, 0.2f, 0.2f);
    m_lightingShader.setVec3("uLight.diffuse", 0.7f, 0.7f, 0.7f);
    m_lightingShader.setVec3("uLight.specular", 1.0f, 1.0f, 1.0f);
    m_lightingShader.setVec3("uViewPos", viewPos.x(), viewPos.y(), viewPos.z());

    QMatrix4x4 model;
    model.rotate(m_time * 20.0f, 0.3f, 1.0f, 0.0f);
    m_lightingShader.setMat4("uModel", model.constData());
    m_lightingShader.setMat4("uView", view.constData());
    m_lightingShader.setMat4("uProjection", projection.constData());
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    m_lightCubeShader.use();
    QMatrix4x4 lightModel;
    lightModel.translate(lightPos);
    lightModel.scale(0.15f);
    m_lightCubeShader.setMat4("uModel", lightModel.constData());
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
