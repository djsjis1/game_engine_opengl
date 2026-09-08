#include "openglwidget.h"
#include <QtMath>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <cmath>

OpenGLWidget::OpenGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { m_angle += 0.01f; update(); });
    m_timer->start(16);
    setFocusPolicy(Qt::StrongFocus);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteVertexArrays(1, &m_lightVAO);
    glDeleteBuffers(1, &m_lightVBO);
    glDeleteTextures(1, &m_normalTex);
    doneCurrent();
}

// 【知识点】程序化生成砖墙法线贴图
// 砖块间隔处法线偏移模拟凹陷，砖块表面有细微凹凸
GLuint OpenGLWidget::generateBrickNormalMap(int w, int h)
{
    QImage image(w, h, QImage::Format_RGB888);

    float brickW = w / 5.0f; // 5 块砖宽
    float brickH = h / 3.0f; // 3 块砖高
    float mortarHalf = 4.0f; // 灰缝半宽
    float brickHalf = 3.0f;  // 砖块内凹凸半宽

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float u = (float)x / (float)w * 5.0f; // 五列砖
            float v = (float)y / (float)h * 3.0f; // 三行砖

            // 判断是否在灰缝区域
            float fx = fmodf(u, 1.0f);
            float fy = fmodf(v, 1.0f);

            // 水平灰缝或垂直灰缝
            bool inMortarX = (fx < mortarHalf / brickW) || (fx > 1.0f - mortarHalf / brickW);
            bool inMortarY = (fy < mortarHalf / brickH) || (fy > 1.0f - mortarHalf / brickH);

            // 小砖块内的小凸起
            bool inBrickX = (fx < brickHalf / brickW) || (fx > 1.0f - brickHalf / brickW);
            bool inBrickY = (fy < brickHalf / brickH) || (fy > 1.0f - brickHalf / brickH);

            // 法线：(0,0,1) 表示平坦（切线空间）
            QVector3D normal(0.0f, 0.0f, 1.0f);
            if (inMortarX && !inMortarY)
                normal = QVector3D(0.4f, 0.0f, 0.9f); // 灰缝侧面
            else if (inMortarY && !inMortarX)
                normal = QVector3D(0.0f, 0.4f, 0.9f);
            else if (inMortarX && inMortarY)
                normal = QVector3D(0.2f, 0.2f, 0.95f); // 十字交叉更凹
            else if (inBrickX && !inBrickY)
                normal = QVector3D(-0.15f, 0.0f, 0.98f); // 砖内小起伏
            else if (inBrickY && !inBrickX)
                normal = QVector3D(0.0f, -0.15f, 0.98f);

            normal.normalize();
            // 编码到 [0,1] RGB
            int r = qBound(0, (int)((normal.x() * 0.5f + 0.5f) * 255), 255);
            int g = qBound(0, (int)((normal.y() * 0.5f + 0.5f) * 255), 255);
            int b = qBound(0, (int)((normal.z() * 0.5f + 0.5f) * 255), 255);
            image.setPixelColor(x, y, QColor(r, g, b));
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    m_normalShader.load(SHADER_DIR "normal_map.vert", SHADER_DIR "normal_map.frag");
    m_lightShader.load(SHADER_DIR "light_cube.vert", SHADER_DIR "light_cube.frag");

    // ---- 方形墙壁 VAO (位置+法线+UV+切线) ----
    // 两面墙：前墙 + 右墙
    // 每个顶点 11 floats: pos(3)+normal(3)+uv(2)+tangent(3)
    float wallVerts[] = {
        // 前墙 (Z=-2, 法线朝前 Z=0,0,1)
        -1, 0, -2, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, -2, 0, 0, 1, 1, 0, 1, 0, 0,
        1, 2, -2, 0, 0, 1, 1, 1, 1, 0, 0,
        -1, 0, -2, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 2, -2, 0, 0, 1, 1, 1, 1, 0, 0,
        -1, 2, -2, 0, 0, 1, 0, 1, 1, 0, 0,
        // 右墙 (X=2, 法线朝右 X=1,0,0)
        2, 0, -2, 1, 0, 0, 0, 0, 0, 0, -1,
        2, 0, 2, 1, 0, 0, 1, 0, 0, 0, -1,
        2, 2, 2, 1, 0, 0, 1, 1, 0, 0, -1,
        2, 0, -2, 1, 0, 0, 0, 0, 0, 0, -1,
        2, 2, 2, 1, 0, 0, 1, 1, 0, 0, -1,
        2, 2, -2, 1, 0, 0, 0, 1, 0, 0, -1};
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wallVerts), wallVerts, GL_STATIC_DRAW);
    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texcoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // tangent
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // ---- 光源指示器 VAO (简单立方体) ----
    float cube[] = {
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f};
    glGenVertexArrays(1, &m_lightVAO);
    glGenBuffers(1, &m_lightVBO);
    glBindVertexArray(m_lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // 生成程序化砖墙法线贴图
    m_normalTex = generateBrickNormalMap(512, 512);
}

void OpenGLWidget::paintGL()
{
    glClearColor(0.05f, 0.06f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 view, projection;
    QVector3D camPos(m_dist * cosf(qDegreesToRadians(m_pitch)) * cosf(qDegreesToRadians(m_yaw)),
                     m_dist * sinf(qDegreesToRadians(m_pitch)),
                     m_dist * cosf(qDegreesToRadians(m_pitch)) * sinf(qDegreesToRadians(m_yaw)));
    view.lookAt(camPos, QVector3D(0.5f, 1, -1), QVector3D(0, 1, 0));
    projection.perspective(45.0f, float(width()) / height(), 0.1f, 100.0f);

    QVector3D lightPos(2.5f * cosf(m_angle * 0.7f), 3.0f, 2.5f * sinf(m_angle * 0.7f));

    // 绑法线纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_normalTex);

    m_normalShader.use();
    m_normalShader.setInt("uNormalMap", 0);
    m_normalShader.setInt("uShowNormalOnly", m_showNormalMap ? 1 : 0);
    m_normalShader.setVec3("uLightPos", lightPos.x(), lightPos.y(), lightPos.z());
    m_normalShader.setVec3("uViewPos", camPos.x(), camPos.y(), camPos.z());
    m_normalShader.setMat4("uView", view.data());
    m_normalShader.setMat4("uProjection", projection.data());
    QMatrix4x4 model;
    m_normalShader.setMat4("uModel", model.data());
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    // 光源指示器
    m_lightShader.use();
    m_lightShader.setMat4("uView", view.data());
    m_lightShader.setMat4("uProjection", projection.data());
    model.setToIdentity();
    model.translate(lightPos);
    model.scale(0.15f);
    m_lightShader.setMat4("uModel", model.data());
    glBindVertexArray(m_lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void OpenGLWidget::resizeGL(int w, int h) {}

void OpenGLWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_lastMouse = e->pos();
        m_rotating = true;
    }
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rotating)
    {
        QPoint d = e->pos() - m_lastMouse;
        m_yaw += d.x() * 0.5f;
        m_pitch -= d.y() * 0.5f;
        m_pitch = qBound(-85.0f, m_pitch, 85.0f);
        m_lastMouse = e->pos();
    }
}

void OpenGLWidget::wheelEvent(QWheelEvent *e) { m_dist = qBound(2.0f, m_dist - e->angleDelta().y() * 0.01f, 30.0f); }

void OpenGLWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_N)
        m_showNormalMap = !m_showNormalMap;
    else if (e->key() == Qt::Key_Escape)
        close();
}
