#ifndef MESHFACTORY_H
#define MESHFACTORY_H

#include <QOpenGLFunctions_3_3_Core>
#include <vector>

// ============================================================
// MeshFactory — 生成基础几何体的 VAO 数据
//
// 每个顶点 = 位置(3 float) + 法线(3 float) = 6 个 float
// 使用索引绘制（24 顶点 + 36 索引）避免重复顶点
// ============================================================
struct Vertex
{
    float px, py, pz; // 位置
    float nx, ny, nz; // 法线
};

class MeshFactory : protected QOpenGLFunctions_3_3_Core
{
public:
    // ---- 创建立方体（中心在原点，边长 1.0） ----
    // 必须在 OpenGL 上下文激活后调用（如 initializeGL 中）
    void createCube(MeshComponent &outMesh)
    {
        if (!m_glInit)
        {
            initializeOpenGLFunctions();
            m_glInit = true;
        }
        //   6 个面 × 4 个顶点 = 24 个独立顶点（每面法线不同）
        //  36 个索引（6 面 × 2 三角形 × 3 索引）
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;

        float h = 0.5f; // 半边长

        // 面数据：{ 法线, 四个角的偏移量 }
        struct Face
        {
            float nx, ny, nz;
            float corners[4][3]; // 4 个角的位置偏移
        };

        Face faces[6] = {
            // 正面 (z+)          // 背面 (z-)
            {0, 0, 1, {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}}},
            {0, 0, -1, {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
            // 右面 (x+)          // 左面 (x-)
            {1, 0, 0, {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}},
            {-1, 0, 0, {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}},
            // 顶面 (y+)          // 底面 (y-)
            {0, 1, 0, {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}}},
            {0, -1, 0, {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}},
        };

        for (int f = 0; f < 6; ++f)
        {
            GLuint base = static_cast<GLuint>(vertices.size());
            for (int v = 0; v < 4; ++v)
            {
                Vertex vert;
                vert.px = faces[f].corners[v][0];
                vert.py = faces[f].corners[v][1];
                vert.pz = faces[f].corners[v][2];
                vert.nx = faces[f].nx;
                vert.ny = faces[f].ny;
                vert.nz = faces[f].nz;
                vertices.push_back(vert);
            }
            // 两个三角形：0-1-2, 0-2-3
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }

        // 上传到 GPU
        glGenVertexArrays(1, &outMesh.vao);
        glGenBuffers(1, &outMesh.vbo);
        glGenBuffers(1, &outMesh.ebo);

        glBindVertexArray(outMesh.vao);

        // VBO: 顶点数据
        glBindBuffer(GL_ARRAY_BUFFER, outMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);

        // EBO: 索引数据
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outMesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(GLuint),
                     indices.data(), GL_STATIC_DRAW);

        // 顶点属性：location=0 → 位置 (3 float)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex), (void *)0);
        glEnableVertexAttribArray(0);

        // 顶点属性：location=1 → 法线 (3 float)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        outMesh.vertexCount = static_cast<GLsizei>(vertices.size());
        outMesh.indexCount = static_cast<GLsizei>(indices.size());
        outMesh.hasIndices = true;
        outMesh.primitiveType = GL_TRIANGLES;
    }

    // ---- 销毁网格资源 ----
    void destroyMesh(MeshComponent &mesh)
    {
        if (!m_glInit)
            return;
        if (mesh.vao)
            glDeleteVertexArrays(1, &mesh.vao);
        if (mesh.vbo)
            glDeleteBuffers(1, &mesh.vbo);
        if (mesh.ebo)
            glDeleteBuffers(1, &mesh.ebo);
        mesh.vao = mesh.vbo = mesh.ebo = 0;
    }

private:
    bool m_glInit = false;
};

#endif
