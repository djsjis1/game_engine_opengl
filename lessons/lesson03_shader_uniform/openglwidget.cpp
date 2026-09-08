#include "openglwidget.h"
#include <cmath>
#include <QDebug>

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // ============================================================
    // 加载着色器（从文件加载，使用Shader工具类）
    // ============================================================
    if (!m_shader.load(SHADER_DIR "triangle.vert", SHADER_DIR "triangle.frag"))
    {
        qDebug() << "ERROR: Failed to load shaders!";
        return;
    }

    // ============================================================
    // 顶点数据（位置 + 颜色）
    // 每个顶点现在有6个float：前3个是位置(x,y,z)，后3个是颜色(r,g,b)
    // ============================================================
    float vertices[] = {
        // 位置              // 颜色
        -0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.0f, // 左下 - 红色
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // 右下 - 绿色
        0.0f,
        0.5f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // 顶部 - 蓝色
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 【知识点】设置顶点属性布局
    // 每个顶点6个float，前3个是位置，后3个是颜色
    int stride = 6 * sizeof(float); // 步长 = 6个float

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    // 颜色属性 (location = 1) - 偏移3个float
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // ============================================================
    // 设置定时器 - 每16ms (约60fps) 触发一次更新
    // ============================================================
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            {
                m_time += 0.016f; // 累计时间增量
                update();         // 触发重绘 (调用paintGL)
            });
    m_timer->start(16);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 激活着色器
    m_shader.use();

    // 【知识点】使用 uniform 变量动态修改着色器参数
    //
    // uMixRatio 随时间正弦变化，在0和1之间来回切换
    // sin函数产生平滑的过渡效果
    // 观察三角形颜色的变化！
    float mixRatio = (sin(m_time * 2.0f) + 1.0f) * 0.5f;
    m_shader.setFloat("uMixRatio", mixRatio);

    // uColor 也随时间变化
    float r = (sin(m_time) + 1.0f) * 0.5f;
    float g = (sin(m_time + 2.0f) + 1.0f) * 0.5f;
    float b = (sin(m_time + 4.0f) + 1.0f) * 0.5f;
    m_shader.setVec3("uColor", r, g, b);

    // 绘制三角形
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
