#ifndef ENGINE_COMPONENTS_MESH_H
#define ENGINE_COMPONENTS_MESH_H

#include <QOpenGLFunctions_3_3_Core>

// ============================================================
// MeshComponent: 网格渲染数据
//
// 存储 OpenGL 缓冲对象（VAO/VBO/EBO）和绘制参数。
// 资源生命周期由创建者管理（通常为 MeshFactory 或用户代码）。
//
// 用法：
//   - 在 initializeGL 中创建 VAO/VBO/EBO
//   - 在析构时调用 glDelete* 释放
// ============================================================
struct MeshComponent
{
    GLuint vao = 0;                      // 顶点数组对象
    GLuint vbo = 0;                      // 顶点缓冲对象
    GLuint ebo = 0;                      // 索引缓冲对象（可选，为 0 表示无索引）
    GLsizei vertexCount = 0;             // 顶点数量（无索引时用于 glDrawArrays）
    GLsizei indexCount = 0;              // 索引数量（有索引时用于 glDrawElements）
    GLenum primitiveType = GL_TRIANGLES; // 图元类型
    bool hasIndices = false;             // 是否使用索引绘制

    // 判断网格是否有效（已创建 VAO）
    bool isValid() const { return vao != 0; }
};

#endif // ENGINE_COMPONENTS_MESH_H
