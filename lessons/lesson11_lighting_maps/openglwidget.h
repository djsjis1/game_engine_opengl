#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include <QImage>
#include "shader.h"

// ============================================================
// Lesson 11: 光照贴图 (Lighting Maps)
// 核心概念：
//   1. 漫反射贴图 (Diffuse Map)   —— 替代材质 diffuse 颜色
//   2. 镜面光贴图 (Specular Map)  —— 控制不同位置的高光强度
//   3. 顶点属性新增 aTexCoord（位置 + 法线 + 纹理坐标 = 8 floats）
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
    GLuint createTexture(const QImage &img);
    QImage makeContainerImage(int size);    // 漫反射贴图（木箱风格）
    QImage makeSpecularMaskImage(int size); // 镜面贴图（边框金属、内部木纹无高光）

    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_lightVAO = 0, m_lightVBO = 0;
    GLuint m_diffuseMap = 0, m_specularMap = 0;
    Shader m_lightingShader;
    Shader m_lightCubeShader;
    QTimer *m_timer = nullptr;
    float m_time = 0.0f;
};

#endif
