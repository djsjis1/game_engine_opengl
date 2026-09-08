#include "openglwidget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QDebug>
#include <QtMath>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // 启用鼠标追踪（无需按下也能接收移动事件）

    // 【知识点】隐藏鼠标光标并锁定在窗口中心，实现"无限旋转"
    setCursor(Qt::BlankCursor);
    // 将鼠标移到窗口中心
    QCursor::setPos(mapToGlobal(QPoint(width() / 2, height() / 2)));
}

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.bits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_shader.load(SHADER_DIR "camera.vert", SHADER_DIR "camera.frag");

    // 立方体顶点（同Lesson07）
    float vertices[] = {
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

    QImage img = createCheckerImage(64, QColor(220, 120, 50), QColor(250, 220, 180));
    m_texture = createTexture(img);

    glEnable(GL_DEPTH_TEST);

    // 初始化摄像机向量
    updateCameraVectors();

    // 启动高精度计时器
    m_elapsed.start();
    m_lastFrameTime = 0;

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { update(); });
    m_timer->start(16);

    qDebug() << "Lesson 08 - WASD to move, Mouse to look around, Scroll to zoom, ESC to exit";
}

void OpenGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void OpenGLWidget::paintGL()
{
    // 【知识点】deltaTime：当前帧时间 - 上一帧时间
    // 用秒为单位，确保不同帧率下移动速度一致
    float currentTime = m_elapsed.elapsed() / 1000.0f;
    float deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    // 处理键盘输入移动
    processKeyboard(deltaTime);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_shader.setInt("uTexture", 0);

    // View矩阵：由摄像机位置和朝向计算
    // 【知识点】lookAt = 摄像机朝向目标方向的矩阵
    // 目标点 = 摄像机位置 + 朝向向量
    QMatrix4x4 view;
    view.lookAt(m_cameraPos, m_cameraPos + m_cameraFront, m_cameraUp);

    // 透视投影
    QMatrix4x4 projection;
    float aspect = float(width()) / float(height());
    projection.perspective(m_fov, aspect, 0.1f, 100.0f);

    // 绘制多个立方体，形成一个"场景"
    m_shader.setMat4("uView", view.constData());
    m_shader.setMat4("uProjection", projection.constData());
    glBindVertexArray(m_vao);

    // 在地面上画一个5×5的立方体阵列
    for (int x = -2; x <= 2; ++x)
    {
        for (int z = -2; z <= 2; ++z)
        {
            QMatrix4x4 model;
            model.translate(x * 2.0f, -1.0f, z * 2.0f); // 排列成网格
            model.rotate(currentTime * 30.0f + (x + z) * 20.0f, 0.5f, 1.0f, 0.0f);
            m_shader.setMat4("uModel", model.constData());
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    glBindVertexArray(0);
}

void OpenGLWidget::keyPressEvent(QKeyEvent *event)
{
    m_keysPressed.insert(event->key());
    if (event->key() == Qt::Key_Escape)
        close();
}

void OpenGLWidget::keyReleaseEvent(QKeyEvent *event)
{
    m_keysPressed.erase(event->key());
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 【知识点】鼠标控制摄像机旋转
    // 基于鼠标位移量来调整 yaw 和 pitch

    if (m_firstMouse)
    {
        m_lastMousePos = event->pos();
        m_firstMouse = false;
        return;
    }

    // 计算鼠标位移
    QPointF offset = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    // 乘以灵敏度
    offset *= m_mouseSensitivity;

    m_yaw += offset.x();
    m_pitch -= offset.y(); // 注意：y轴反转（鼠标上移=抬头）

    // 【知识点】限制俯仰角，防止翻转
    if (m_pitch > 89.0f)
        m_pitch = 89.0f;
    if (m_pitch < -89.0f)
        m_pitch = -89.0f;

    // 更新摄像机向量
    updateCameraVectors();

    // 将鼠标重置到窗口中心（实现无限旋转）
    QPoint center = mapToGlobal(QPoint(width() / 2, height() / 2));
    QCursor::setPos(center);
    m_lastMousePos = QPoint(width() / 2, height() / 2);
}

void OpenGLWidget::wheelEvent(QWheelEvent *event)
{
    // 【知识点】滚轮调整视野(FOV)
    m_fov -= event->angleDelta().y() / 120.0f; // 每格调整1度
    if (m_fov < 1.0f)
        m_fov = 1.0f;
    if (m_fov > 120.0f)
        m_fov = 120.0f;
}

void OpenGLWidget::updateCameraVectors()
{
    // 【知识点】根据yaw和pitch计算摄像机朝向
    // 这是FPS摄像机的核心数学
    QVector3D front;
    front.setX(cos(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    front.setY(sin(qDegreesToRadians(m_pitch)));
    front.setZ(sin(qDegreesToRadians(m_yaw)) * cos(qDegreesToRadians(m_pitch)));
    m_cameraFront = front.normalized();

    // 右方向 = 朝向 × 世界上方向（叉积）
    m_cameraRight = QVector3D::crossProduct(m_cameraFront, m_worldUp).normalized();

    // 摄像机上方向 = 右方向 × 朝向（叉积）
    m_cameraUp = QVector3D::crossProduct(m_cameraRight, m_cameraFront).normalized();
}

void OpenGLWidget::processKeyboard(float dt)
{
    // 【知识点】基于deltaTime的移动
    // velocity = speed * dt，保证不同帧率下移动速度一致
    float velocity = m_movementSpeed * dt;

    if (m_keysPressed.count(Qt::Key_W))
        m_cameraPos += m_cameraFront * velocity;
    if (m_keysPressed.count(Qt::Key_S))
        m_cameraPos -= m_cameraFront * velocity;
    if (m_keysPressed.count(Qt::Key_A))
        m_cameraPos -= m_cameraRight * velocity;
    if (m_keysPressed.count(Qt::Key_D))
        m_cameraPos += m_cameraRight * velocity;
    if (m_keysPressed.count(Qt::Key_Space))
        m_cameraPos += m_worldUp * velocity; // 上升
    if (m_keysPressed.count(Qt::Key_Shift))
        m_cameraPos -= m_worldUp * velocity; // 下降
}
