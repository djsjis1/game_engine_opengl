#ifndef MESHCOMPONENT_H
#define MESHCOMPONENT_H

#include <QOpenGLFunctions_3_3_Core>
struct MeshComponent
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei vertexCount = 0;
    GLsizei indexCount = 0;
    GLenum primitiveType = GL_TRIANGLES;
    bool hasIndices = false;

    bool isValid() const { return vao != 0; }
};

#endif
