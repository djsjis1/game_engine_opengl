#include "openglwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent) { setFocusPolicy(Qt::StrongFocus); }
OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_ebo)
        glDeleteBuffers(1, &m_ebo);
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    doneCurrent();
}

QImage OpenGLWidget::createCheckerImage(int size, QColor c1, QColor c2)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    int half = size / 2;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            img.setPixelColor(x, y, ((x < half) ^ (y < half)) ? c1 : c2);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.bits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_shader.load(SHADER_DIR "transform.vert", SHADER_DIR "transform.frag");

    // 顶点：位置 + 纹理坐标
    float vertices[] = {
        // 位置               // 纹理坐标
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
    };
    unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    int stride = 5 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    QImage img = createCheckerImage(128, QColor(200, 100, 50), QColor(50, 100, 200));
    m_texture = createTexture(img);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_shader.setInt("uTexture", 0);

    // ============================================================
    // 【知识点】使用 QMatrix4x4 构建变换矩阵
    // Qt 的矩阵操作与 glm 类似，使用链式调用
    //
    // 【重要】变换顺序：先缩放 → 再旋转 → 最后平移
    // 数学上：M = T * R * S（从右到左应用）
    //
    // QMatrix4x4 默认是单位矩阵，setToIdentity() 重置
    // ============================================================
    QMatrix4x4 model;
    model.setToIdentity();

    // 1. 平移到(0.5, -0.3, 0)位置 — 移到右下角
    model.translate(-0.5f, -0.3f, 0.0f);

    // 2. 绕Z轴旋转（随时间变化）
    model.rotate(m_time * 180.0f, 0.0f, 0.0f, 1.0f); // 旋转角度用"度"！不是弧度！

    // 3. 缩放到原始大小的60%
    model.scale(0.6f, 0.6f, 1.0f);

    // 将矩阵传递给着色器
    m_shader.setMat4("uModel", model.constData());

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
