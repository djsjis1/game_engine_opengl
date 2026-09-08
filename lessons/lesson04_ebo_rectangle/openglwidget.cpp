#include "openglwidget.h"
#include <QKeyEvent>
#include <QDebug>

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // 启用键盘焦点（必须！否则收不到键盘事件）
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
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // 加载着色器
    m_shader.load(SHADER_DIR "rectangle.vert", SHADER_DIR "rectangle.frag");

    // ============================================================
    // 顶点数据：矩形4个角（每个顶点：位置xyz + 颜色rgb）
    // 四个角分别用红、绿、蓝、黄表示
    // ============================================================
    float vertices[] = {
        // 位置                // 颜色
        -0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        0.0f, // 0: 左下 - 红色
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // 1: 右下 - 绿色
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // 2: 右上 - 蓝色
        -0.5f,
        0.5f,
        0.0f,
        1.0f,
        1.0f,
        0.0f, // 3: 左上 - 黄色
    };

    // ============================================================
    // 【知识点】索引数据
    // 用unsigned int（GLuint）存储顶点索引
    // 每3个索引组成一个三角形
    // OpenGL按逆时针方向判断三角形的"正面"
    // ============================================================
    unsigned int indices[] = {
        0, 1, 2, // 第一个三角形：左下→右下→右上
        0, 2, 3  // 第二个三角形：左下→右上→左上（共享顶点0和2）
    };

    // 创建VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // 创建VBO并上传顶点数据
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 【知识点】创建EBO并上传索引数据
    // GL_ELEMENT_ARRAY_BUFFER: 专门用于索引数据的缓冲类型
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 设置顶点属性
    int stride = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑VAO（EBO会被VAO记录，所以先解绑VAO再解绑EBO/VBO没关系）
    glBindVertexArray(0);

    qDebug() << "Lesson 04 - Press 'W' key to toggle wireframe mode!";
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 【知识点】设置多边形绘制模式
    // GL_FILL:  填充模式（默认）
    // GL_LINE:  线框模式 — 可以看到三角形的边！
    // GL_POINT: 点模式
    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_shader.use();
    glBindVertexArray(m_vao);

    // 【知识点】glDrawElements - 按索引绘制
    // 参数: (图元类型, 索引数量, 索引数据类型, 偏移量)
    // 6个索引 = 2个三角形 = 1个矩形
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    // 【知识点】按 W 键切换线框/填充模式
    if (event->key() == Qt::Key_W)
    {
        m_wireframe = !m_wireframe;
        qDebug() << "Wireframe mode:" << (m_wireframe ? "ON" : "OFF");
        update(); // 触发重绘
    }

    // 按 ESC 关闭窗口
    if (event->key() == Qt::Key_Escape)
    {
        close();
    }
}
