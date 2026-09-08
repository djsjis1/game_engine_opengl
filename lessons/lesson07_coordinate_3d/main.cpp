// ============================================================
// Lesson 07: 3D坐标系统
// 知识点：
// 1. MVP矩阵：Model × View × Projection
// 2. QMatrix4x4::lookAt() 摄像机朝向
// 3. QMatrix4x4::perspective() 透视投影（近大远小）
// 4. glEnable(GL_DEPTH_TEST) 深度测试
// 5. 36个顶点构成完整立方体
// 6. 变换流水线：局部→世界→观察→裁剪→屏幕
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
    format.setDepthBufferSize(24);  // 必须设置深度缓冲大小
    QSurfaceFormat::setDefaultFormat(format);

    OpenGLWidget widget;
    widget.resize(800, 600);
    widget.setWindowTitle("Lesson 07 - 3D Coordinate Systems");
    widget.show();

    return app.exec();
}
