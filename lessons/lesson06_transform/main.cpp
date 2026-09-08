// ============================================================
// Lesson 06: 变换 (Transformations)
// 知识点：
// 1. QMatrix4x4 矩阵操作（Qt内置，无需glm）
//    - translate() 平移
//    - rotate()    旋转（角度制）
//    - scale()     缩放
// 2. 变换顺序：先缩放→再旋转→最后平移
// 3. 矩阵通过uniform传递给顶点着色器
// 4. 绘制一个旋转、缩放、偏移的纹理矩形
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
    QSurfaceFormat::setDefaultFormat(format);

    OpenGLWidget widget;
    widget.resize(800, 600);
    widget.setWindowTitle("Lesson 06 - Transformations");
    widget.show();

    return app.exec();
}
