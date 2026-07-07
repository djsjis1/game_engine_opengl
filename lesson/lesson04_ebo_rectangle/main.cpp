// ============================================================
// Lesson 04: 索引缓冲对象 (EBO)
// 知识点：
// 1. EBO复用顶点（4个顶点+6个索引=矩形）
// 2. glDrawElements按索引绘制
// 3. glPolygonMode切换线框/填充模式
// 4. 键盘事件处理（按W切换线框，ESC退出）
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
    widget.setWindowTitle("Lesson 04 - EBO Rectangle (Press W for wireframe)");
    widget.show();

    return app.exec();
}
