#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QTimer>
#include "shader.h"

// ============================================================
// Lesson 05: 纹理贴图
// 核心概念：
//   1. 纹理坐标 (UV坐标) - (0,0)左下角 → (1,1)右上角
//   2. 纹理单元 (Texture Unit) - GPU上的纹理"插槽"
//   3. 纹理采样器 (sampler2D) - 着色器中访问纹理的变量
//   4. QImage生成纹理 - 程序化生成纹理图片
//   5. 多纹理混合 - 两张纹理叠加效果
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

private:
    GLuint createTexture(const QImage& image);  // 从QImage创建OpenGL纹理
    QImage createCheckerImage(int size, QColor c1, QColor c2);  // 生成棋盘格

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLuint m_texture1 = 0;   // 纹理1 ID
    GLuint m_texture2 = 0;   // 纹理2 ID
    Shader  m_shader;
    QTimer* m_timer = nullptr;
    float   m_time = 0.0f;
};

#endif
