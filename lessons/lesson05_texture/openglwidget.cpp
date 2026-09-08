#include "openglwidget.h"
#include <QKeyEvent>
#include <QDebug>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_ebo)
        glDeleteBuffers(1, &m_ebo);
    if (m_texture1)
        glDeleteTextures(1, &m_texture1);
    if (m_texture2)
        glDeleteTextures(1, &m_texture2);
    doneCurrent();
}

// 生成棋盘格纹理图像
QImage OpenGLWidget::createCheckerImage(int size, QColor c1, QColor c2)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    int half = size / 2;
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            bool isC1 = ((x < half) ^ (y < half)); // XOR产生棋盘格
            img.setPixelColor(x, y, isC1 ? c1 : c2);
        }
    }
    return img;
}

// 从QImage创建OpenGL纹理
GLuint OpenGLWidget::createTexture(const QImage &image)
{
    // 【知识点】OpenGL要求纹理图像的y轴从底部开始
    // QImage的y轴从顶部开始，所以需要翻转
    QImage flipped = image.mirrored(false, true);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // 【知识点】设置纹理环绕方式
    // GL_REPEAT:            重复纹理（默认）
    // GL_MIRRORED_REPEAT:   镜像重复
    // GL_CLAMP_TO_EDGE:     边缘拉伸（超出部分用边缘颜色）
    // GL_CLAMP_TO_BORDER:   超出部分用指定边界颜色
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // 【知识点】设置纹理过滤方式
    // GL_NEAREST: 最近邻过滤（像素风格，锯齿明显）
    // GL_LINEAR:  线性过滤（平滑模糊，效果更好）
    // 可以分别设置放大(MAG)和缩小(MIN)过滤
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 【知识点】上传图像数据到GPU
    // 参数: (纹理目标, mipmap级别, 内部格式, 宽, 高, 0, 源格式, 数据类型, 数据指针)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.bits());

    // 【知识点】生成Mipmap（多级渐远纹理）
    // Mipmap是一系列逐渐缩小的纹理图像，用于远处物体的纹理采样
    // 可以提高渲染性能并减少闪烁
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    m_shader.load(SHADER_DIR "texture.vert", SHADER_DIR "texture.frag");

    // ============================================================
    // 顶点数据：矩形（位置xyz + 纹理坐标uv）
    // 【知识点】纹理坐标 (UV坐标)
    //   u（水平）: 0.0=左边, 1.0=右边
    //   v（垂直）: 0.0=底部, 1.0=顶部
    //   原点在左下角！
    // ============================================================
    float vertices[] = {
        // 位置               // 纹理坐标
        -0.5f,
        -0.5f,
        0.0f,
        0.0f,
        0.0f, // 左下
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f, // 右下
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        1.0f, // 右上
        -0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f, // 左上
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

    // 顶点属性布局：5个float/顶点 (3位置 + 2纹理坐标)
    int stride = 5 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // ============================================================
    // 创建两张程序化纹理
    // ============================================================
    // 纹理1：棋盘格（红/蓝交替）
    QImage checkerImg = createCheckerImage(256, QColor(200, 50, 50), QColor(50, 50, 200));
    m_texture1 = createTexture(checkerImg);

    // 纹理2：棋盘格（绿/黄交替，较小的格子）
    QImage checkerImg2 = createCheckerImage(64, QColor(50, 200, 50), QColor(200, 200, 50));
    m_texture2 = createTexture(checkerImg2);

    // 设置定时器，用于动态调整纹理混合比例
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
        m_time += 0.016f;
        update(); });
    m_timer->start(16);

    qDebug() << "Lesson 05 - Press Up/Down arrow keys to adjust mix ratio!";
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader.use();

    // 【知识点】纹理单元的用法
    // 1. glActiveTexture(GL_TEXTURE0 + i) 激活纹理单元i
    // 2. glBindTexture 将纹理绑定到当前激活的单元
    // 3. 通过uniform将纹理单元编号传给着色器
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture1);
    m_shader.setInt("uTexture1", 0); // 告诉着色器：uTexture1用纹理单元0

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture2);
    m_shader.setInt("uTexture2", 1); // 告诉着色器：uTexture2用纹理单元1

    // 混合比例在0~1之间循环
    float mixRatio = (sin(m_time) + 1.0f) * 0.5f;
    m_shader.setFloat("uMixRatio", mixRatio);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
}
