// ============================================================
// Lesson 11: 光照贴图 (Lighting Maps)
// 知识点：
//   - 漫反射贴图：用纹理代替材质 diffuse 颜色
//   - 镜面光贴图：黑色区域无高光，白色区域产生强高光
//   - 顶点属性扩展为 (位置 + 法线 + 纹理坐标)
// 操作：Esc 退出
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
    widget.resize(800, 600);
    widget.setWindowTitle("Lesson 11 - Lighting Maps (漫反射 + 镜面贴图)");
    widget.show();
    return app.exec();
}
