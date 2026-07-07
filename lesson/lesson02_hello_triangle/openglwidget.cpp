#include "openglwidget.h"
#include <QDebug>

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

OpenGLWidget::~OpenGLWidget()
{
    // 清理GPU资源（在上下文还存在时清理）
    makeCurrent();
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_shaderProgram)
        glDeleteProgram(m_shaderProgram);
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // ============================================================
    // 第1步：准备顶点数据
    // ============================================================
    // 【知识点】OpenGL使用标准化设备坐标(NDC)
    //   x轴: -1.0(左) ~ 1.0(右)
    //   y轴: -1.0(底) ~ 1.0(顶)
    //   z轴: -1.0(后) ~ 1.0(前)
    //
    // 【知识点】长方形 = 两个三角形拼接
    //   三角形1: 左下 → 右下 → 右上
    //   三角形2: 左下 → 右上 → 左上
    float vertices[] = {
        -0.5f, -0.3f, 0.0f, // 左下角
        0.5f, -0.3f, 0.0f,  // 右下角
        0.5f, 0.3f, 0.0f,   // 右上角
        -0.5f, -0.3f, 0.0f, // 左下角（重复，拼第二个三角形）
        0.5f, 0.3f, 0.0f,   // 右上角（重复）
        -0.5f, 0.3f, 0.0f   // 左上角
    };

    // ============================================================
    // 第2步：创建VAO和VBO
    // ============================================================

    // 【知识点】VAO (Vertex Array Object) - 顶点数组对象
    // 存储所有顶点属性指针的配置，像一个"配置快照"
    // 绑定VAO后，后续的VBO绑定、属性设置都会被记录在VAO中
    glGenVertexArrays(1, &m_vao); // 生成VAO
    glBindVertexArray(m_vao);     // 绑定VAO

    // 【知识点】VBO (Vertex Buffer Object) - 顶点缓冲对象
    // 在GPU显存中分配一块区域来存储顶点数据
    // GL_ARRAY_BUFFER: 表示这个缓冲用于顶点属性数据
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 【知识点】glVertexAttribPointer - 告诉OpenGL如何解析顶点数据
    // 参数: (属性位置, 分量数量, 数据类型, 是否归一化, 步长, 偏移量)
    // 每个顶点有3个float(x,y,z)，紧密排列，步长=3*sizeof(float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0); // 启用属性位置0

    // 解绑VAO和VBO（良好的习惯，防止误操作）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ============================================================
    // 第3步：编译着色器程序
    // ============================================================

    // 顶点着色器源码（内嵌，方便学习）
    // 实际项目中通常从文件加载
    // 注意：不能用 R"(...)" ，MSVC 预处理器会误解析 #version
    const char *vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos, 1.0);\n"
        "}\n";

    // 片段着色器源码
    const char *fragmentSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = vec4(1.0, 0.5, 0.2, 1.0);\n"
        "}\n";

    m_shaderProgram = createShaderProgram(vertexSource, fragmentSource);
    if (m_shaderProgram == 0)
    {
        qDebug() << "ERROR: Failed to create shader program!";
    }
}

GLuint OpenGLWidget::createShaderProgram(const char *vertexSource, const char *fragmentSource)
{
    // ---- 编译顶点着色器 ----
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    // 检查编译错误
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        qDebug() << "Vertex Shader Compile Error:\n"
                 << infoLog;
    }

    // ---- 编译片段着色器 ----
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        qDebug() << "Fragment Shader Compile Error:\n"
                 << infoLog;
    }

    // ---- 链接着色器程序 ----
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        qDebug() << "Shader Program Link Error:\n"
                 << infoLog;
    }

    // 链接完成后可以删除着色器对象（程序已包含编译后的代码）
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    // 清屏
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 【知识点】使用着色器程序
    glUseProgram(m_shaderProgram);

    // 【知识点】绑定VAO（它记录了顶点属性配置）
    glBindVertexArray(m_vao);

    // 【知识点】glDrawArrays - 绘制图元
    // 参数: (图元类型, 起始顶点索引, 顶点数量)
    // GL_TRIANGLES: 每3个顶点组成一个三角形
    // 长方形 = 2个三角形 = 6个顶点
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 解绑
    glBindVertexArray(0);
    glUseProgram(0);
}
