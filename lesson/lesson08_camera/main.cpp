// ============================================================
// Lesson 08: 摄像机 (Camera)
// 知识点：
// 1. 欧拉角 (yaw/pitch) 控制摄像机方向
// 2. WASD 前后左右移动
// 3. 鼠标控制视角旋转（隐藏光标 + 中心重置）
// 4. deltaTime 保证帧率无关的移动速度
// 5. 滚轮调整FOV（视野范围）
// 6. 5×5立方体阵列场景
// ============================================================

#include <QApplication>
#include <QSurfaceFormat>
#include "openglwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    OpenGLWidget widget;
    widget.resize(1024, 768);
    widget.setWindowTitle("Lesson 08 - FPS Camera (WASD+Mouse)");
    widget.show();

    return app.exec();
}
