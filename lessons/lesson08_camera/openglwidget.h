#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector3D>
#include <set>
#include "shader.h"

// ============================================================
// Lesson 08: 摄像机 (Camera)
// 核心概念：
//   1. 摄像机属性：位置、朝向、上方向
//   2. 欧拉角：yaw(偏航/左右转)、pitch(俯仰/上下看)
//   3. WASD移动 + 鼠标旋转视角
//   4. deltaTime - 帧间时间差，保证移动速度与帧率无关
//   5. 鼠标锁定 - setCursor/QCursor 实现无限旋转
//
// 摄像机向量计算：
//   front.x = cos(yaw)*cos(pitch)
//   front.y = sin(pitch)
//   front.z = sin(yaw)*cos(pitch)
//   right = normalize(cross(front, worldUp))
//   up = normalize(cross(right, front))
// ============================================================
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QImage createCheckerImage(int size, QColor c1, QColor c2);
    GLuint createTexture(const QImage &image);
    void updateCameraVectors();     // 根据yaw/pitch更新摄像机向量
    void processKeyboard(float dt); // 处理键盘移动

    // OpenGL资源
    GLuint m_vao = 0, m_vbo = 0;
    GLuint m_texture = 0;
    Shader m_shader;

    // 定时器
    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsed;   // 高精度计时器
    float m_lastFrameTime = 0; // 上一帧时间

    // 摄像机参数
    QVector3D m_cameraPos = QVector3D(0.0f, 0.0f, 5.0f);    // 摄像机位置
    QVector3D m_cameraFront = QVector3D(0.0f, 0.0f, -1.0f); // 朝向
    QVector3D m_cameraUp = QVector3D(0.0f, 1.0f, 0.0f);     // 上方向
    QVector3D m_cameraRight;                                // 右方向（计算得出）
    QVector3D m_worldUp = QVector3D(0.0f, 1.0f, 0.0f);      // 世界上方向

    float m_yaw = -90.0f; // 偏航角（度）
    float m_pitch = 0.0f; // 俯仰角（度）
    float m_fov = 45.0f;  // 视野角度

    float m_mouseSensitivity = 0.1f; // 鼠标灵敏度
    float m_movementSpeed = 3.0f;    // 移动速度

    // 键盘状态
    std::set<int> m_keysPressed; // 当前按下的键

    bool m_firstMouse = true;
    QPointF m_lastMousePos; // 上一帧鼠标位置
};

#endif
