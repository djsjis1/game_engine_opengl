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
    m_shader.load(SHADER_DIR "cube.vert", SHADER_DIR "cube.frag");

    // 【知识点】立方体36个顶点（6个面，每面2个三角形）
    // 每个顶点：位置(xyz) + 纹理坐标(uv)
    // 纹理坐标使得每个面上都完整显示棋盘格
    float vertices[] = {
        // 前面 (z = 0.5)
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        // 后面 (z = -0.5)
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        -0.5f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        -0.5f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        // 左面 (x = -0.5)
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        -0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        -0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        // 右面 (x = 0.5)
        0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        // 底面 (y = -0.5)
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        1.0f,
        // 顶面 (y = 0.5)
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        -0.5f,
        1.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    int stride = 5 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // 创建纹理
    QImage img = createCheckerImage(128, QColor(220, 120, 50), QColor(250, 220, 180));
    m_texture = createTexture(img);

    // 【知识点】开启深度测试
    // OpenGL默认不测试深度，后绘制的物体会覆盖先绘制的
    // 开启后，OpenGL会记录每个像素的深度值，正确处理前后关系
    glEnable(GL_DEPTH_TEST);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_time += 0.016f; update(); });
    m_timer->start(16);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    // 【知识点】同时清除颜色和深度缓冲
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_shader.setInt("uTexture", 0);

    // ============================================================
    // 【知识点】构建MVP矩阵
    // ============================================================

    // 1. Model矩阵：绕Y轴和X轴旋转
    QMatrix4x4 model;
    model.rotate(m_time * 55.0f, 0.5f, 1.0f, 0.0f); // 绕倾斜轴旋转

    // 2. View矩阵：摄像机位置
    // 【知识点】lookAt(摄像机位置, 注视目标, 上方向)
    // 摄像机在(2, 1.5, 3)看向原点(0,0,0)，头顶朝向y轴
    QMatrix4x4 view;
    view.lookAt(QVector3D(2.0f, 1.5f, 3.0f),  // 眼睛位置
                QVector3D(0.0f, 0.0f, 0.0f),  // 看向哪里
                QVector3D(0.0f, 1.0f, 0.0f)); // 头顶方向

    // 3. Projection矩阵：透视投影
    QMatrix4x4 projection;
    // 【知识点】perspective(FOV, 宽高比, 近平面, 远平面)
    // FOV=45° 视野角度（越大越广角）
    // aspect=宽/高
    // near=0.1 离摄像机0.1以内看不见
    // far=100  离摄像机100以外看不见
    float aspect = float(width()) / float(height());
    projection.perspective(45.0f, aspect, 0.1f, 100.0f);

    // 传递3个矩阵
    m_shader.setMat4("uModel", model.constData());
    m_shader.setMat4("uView", view.constData());
    m_shader.setMat4("uProjection", projection.constData());

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36); // 36个顶点 = 12个三角形 = 6个面
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
