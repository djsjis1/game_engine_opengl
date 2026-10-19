#ifndef MESHFACTORY_H
#define MESHFACTORY_H

#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include <vector>

// ============================================================
// GpuMesh — 一份网格的 GPU 资源（VAO/VBO/EBO）
//
// 注意：这不是组件！它由 ResourceManager 持有并共享，
// 实体的 MeshComponent 只存句柄（meshHandle）指向它。
// ============================================================
struct GpuMesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei vertexCount = 0;
    GLsizei indexCount = 0;
    GLenum primitiveType = GL_TRIANGLES;
    bool hasIndices = false;
    glm::vec3 boundsMin{-0.5f}; // mesh-local AABB for conservative view culling
    glm::vec3 boundsMax{0.5f};
};

// ============================================================
// GpuTexture — 一张纹理的 GPU 资源（精灵帧动画用）
// 由 ResourceManager 持有共享，实体只存句柄
// ============================================================
struct GpuTexture
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

// ============================================================
// MeshFactory — 生成基础几何体/纹理的 GPU 数据（ResourceManager 内部使用）
//
// 每个顶点 = 位置(3) + 法线(3) + UV(2) = 8 个 float
// 使用索引绘制（24 顶点 + 36 索引）避免重复顶点
// ============================================================
struct Vertex
{
    float px, py, pz; // 位置
    float nx, ny, nz; // 法线
    float u, v;       // 纹理坐标（每面 0..1 标准映射）
};

class MeshFactory : protected QOpenGLFunctions_3_3_Core
{
public:
    // ---- 创建立方体（中心在原点，边长 1.0） ----
    // 必须在 OpenGL 上下文激活后调用（如 makeCurrent 中）
    void createCube(GpuMesh &outMesh)
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
            // 每面四角的标准 UV（与 corners 顺序对应：左下→右下→右上→左上）
            const float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (int v = 0; v < 4; ++v)
            {
                Vertex vert;
                vert.px = faces[f].corners[v][0];
                vert.py = faces[f].corners[v][1];
                vert.pz = faces[f].corners[v][2];
                vert.nx = faces[f].nx;
                vert.ny = faces[f].ny;
                vert.nz = faces[f].nz;
                vert.u = uvs[v][0];
                vert.v = uvs[v][1];
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

        // 顶点属性：location=2 → UV (2 float)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        outMesh.vertexCount = static_cast<GLsizei>(vertices.size());
        outMesh.indexCount = static_cast<GLsizei>(indices.size());
        outMesh.hasIndices = true;
        outMesh.primitiveType = GL_TRIANGLES;
        outMesh.boundsMin = glm::vec3(-h);
        outMesh.boundsMax = glm::vec3(h);
    }

    // ---- 销毁网格 GPU 资源 ----
    void destroyMesh(GpuMesh &mesh)
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

    // ---- 从顶点数组创建（OBJ 模型等非索引三角形数据，布局同 cube）----
    // 必须在 GL 上下文内调用
    void createFromVertices(GpuMesh &outMesh, const std::vector<Vertex> &vertices,
                            const glm::vec3 &boundsMin, const glm::vec3 &boundsMax)
    {
        if (!m_glInit)
        {
            initializeOpenGLFunctions();
            m_glInit = true;
        }
        glGenVertexArrays(1, &outMesh.vao);
        glGenBuffers(1, &outMesh.vbo);
        glBindVertexArray(outMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, outMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);
        // 与 createCube 完全一致的属性布局 → 共用 default shader 全套光照
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);

        outMesh.vertexCount = static_cast<GLsizei>(vertices.size());
        outMesh.indexCount = 0;
        outMesh.hasIndices = false; // 非索引：glDrawArrays
        outMesh.primitiveType = GL_TRIANGLES;
        outMesh.boundsMin = boundsMin;
        outMesh.boundsMax = boundsMax;
    }

    // ---- 创建纹理（从 RGBA 像素数据，QImage 解码在 ResourceManager 层）----
    // 必须在 GL 上下文内调用（同 createCube）
    void createTexture(GpuTexture &out, const unsigned char *rgba, int w, int h)
    {
        if (!m_glInit)
        {
            initializeOpenGLFunctions();
            m_glInit = true;
        }
        glGenTextures(1, &out.id);
        glBindTexture(GL_TEXTURE_2D, out.id);
        // 像素风清晰：近邻采样（精灵图放大不模糊，对标 Godot 的 Nearest）
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glBindTexture(GL_TEXTURE_2D, 0);
        out.width = w;
        out.height = h;
    }

    void destroyTexture(GpuTexture &tex)
    {
        if (!m_glInit)
            return;
        if (tex.id)
            glDeleteTextures(1, &tex.id);
        tex.id = 0;
    }

private:
    bool m_glInit = false;
};

#endif
