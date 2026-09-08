// ============================================================
// Lesson 05: 纹理贴图
// 知识点：
// 1. 纹理坐标 (UV) - (0,0)到(1,1)映射
// 2. 纹理单元 + 纹理采样器
// 3. QImage程序化生成纹理
// 4. 纹理参数（环绕方式、过滤方式）
// 5. Mipmap多级渐远纹理
// 6. 多纹理混合
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
    widget.setWindowTitle("Lesson 05 - Texture Mapping");
    widget.show();

    return app.exec();
}
