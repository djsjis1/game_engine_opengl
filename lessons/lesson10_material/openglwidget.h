#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QTimer>
#include <QVector3D>
#include <QVector>
#include "shader.h"

struct MaterialData
{
    QVector3D ambient;
    QVector3D diffuse;
    QVector3D specular;
    float shininess;
};

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
    void setMaterial(const MaterialData &mat);
    GLuint m_cubeVAO = 0, m_cubeVBO = 0;
    GLuint m_lightVAO = 0, m_lightVBO = 0;
    Shader m_materialShader;
    Shader m_lightCubeShader;
    QTimer *m_timer = nullptr;
    float m_time = 0.0f;
    QVector<MaterialData> m_materials;
};
#endif
