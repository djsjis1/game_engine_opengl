#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include "shader.h"

// ============================================================
// Lesson 04: 索引缓冲对象 (EBO)
// 核心概念：
//   1. EBO (Element Buffer Object) - 索引缓冲，复用顶点
//   2. glDrawElements() - 按索引绘制，减少重复顶点
//   3. 线框模式 - glPolygonMode()
//   4. 键盘事件 - 切换线框/填充模式
//
// 为什么需要EBO？
//   画一个矩形需要2个三角形 = 6个顶点
//   但矩形只有4个不同的顶点（4个角）
//   EBO让我们只存4个顶点，用索引指定绘制顺序
//   顶点数据：0(左下) 1(右下) 2(右上) 3(左上)
//   索引数据：0→1→2(△1), 0→2→3(△2)
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
    void keyPressEvent(QKeyEvent *event) override;  // 键盘事件

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;     // 索引缓冲对象（新增！）
    Shader  m_shader;
    bool    m_wireframe = false;  // 是否线框模式
};

#endif
