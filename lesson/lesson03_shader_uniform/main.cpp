// ============================================================
// Lesson 03: 着色器与Uniform
// 知识点：
// 1. 多个顶点属性（位置 + 颜色）
// 2. 插值 - 三角形内部颜色自动渐变
// 3. uniform变量 - CPU向GPU传递全局参数
// 4. Shader工具类 - 从文件加载着色器
// 5. QTimer定时更新 - 实现动态效果
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
    widget.setWindowTitle("Lesson 03 - Shader & Uniform");
    widget.show();

    return app.exec();
}
