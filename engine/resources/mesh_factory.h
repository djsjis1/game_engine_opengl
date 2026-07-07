#ifndef ENGINE_RESOURCES_MESH_FACTORY_H
#define ENGINE_RESOURCES_MESH_FACTORY_H

#include <QOpenGLFunctions_3_3_Core>
#include <QVector3D>
#include <QVector2D>
#include <vector>
#include <cmath>
#include "../components/mesh.h"

// MSVC 默认不定义 M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// MeshFactory: 程序化几何体生成器
//
// 功能：生成常用几何体的顶点数据，创建 VAO/VBO/EBO。
// 所有几何体包含 position(3) + normal(3) + texcoord(2) = 8 floats。
//
// 生成的几何体需要调用者在 OpenGL 上下文就绪后使用，
// 并在析构前调用 destroyMesh() 释放 GL 资源。
//
// 使用示例：
//   MeshFactory factory;
//   MeshComponent cube = factory.createCube();
//   MeshComponent sphere = factory.createSphere(1.0f, 32);
// ============================================================

// 单个顶点的完整布局
struct Vertex
{
    float posX, posY, posZ;
    float normX, normY, normZ;
    float texU, texV;

    Vertex(float px, float py, float pz,
           float nx, float ny, float nz,
           float tu, float tv)
        : posX(px), posY(py), posZ(pz), normX(nx), normY(ny), normZ(nz), texU(tu), texV(tv) {}
};

class MeshFactory : protected QOpenGLFunctions_3_3_Core
{
public:
    MeshFactory() = default;

    // 确保 OpenGL 函数已加载（懒初始化，首次使用时自动调用）
    void ensureInit()
    {
        if (!m_initialized)
        {
            initializeOpenGLFunctions();
            m_initialized = true;
        }
    }

    // ---- 创建立方体 (1x1x1) ----
    // 面朝外的立方体，带法线和 UV
    MeshComponent createCube(float size = 1.0f)
    {
        float h = size * 0.5f;

        // 36 个顶点（6 面 × 2 三角形 × 3 顶点）
        // 格式：pos(3) + normal(3) + uv(2)
        float vertices[] = {
            // 前面 (0,0,-1)
            -h,
            -h,
            h,
            0,
            0,
            1,
            0,
            0,
            h,
            -h,
            h,
            0,
            0,
            1,
            1,
            0,
            h,
            h,
            h,
            0,
            0,
            1,
            1,
            1,
            -h,
            -h,
            h,
            0,
            0,
            1,
            0,
            0,
            h,
            h,
            h,
            0,
            0,
            1,
            1,
            1,
            -h,
            h,
            h,
            0,
            0,
            1,
            0,
            1,
            // 后面 (0,0,-1)
            h,
            -h,
            -h,
            0,
            0,
            -1,
            0,
            0,
            -h,
            -h,
            -h,
            0,
            0,
            -1,
            1,
            0,
            -h,
            h,
            -h,
            0,
            0,
            -1,
            1,
            1,
            h,
            -h,
            -h,
            0,
            0,
            -1,
            0,
            0,
            -h,
            h,
            -h,
            0,
            0,
            -1,
            1,
            1,
            h,
            h,
            -h,
            0,
            0,
            -1,
            0,
            1,
            // 左面 (-1,0,0)
            -h,
            -h,
            -h,
            -1,
            0,
            0,
            0,
            0,
            -h,
            -h,
            h,
            -1,
            0,
            0,
            1,
            0,
            -h,
            h,
            h,
            -1,
            0,
            0,
            1,
            1,
            -h,
            -h,
            -h,
            -1,
            0,
            0,
            0,
            0,
            -h,
            h,
            h,
            -1,
            0,
            0,
            1,
            1,
            -h,
            h,
            -h,
            -1,
            0,
            0,
            0,
            1,
            // 右面 (1,0,0)
            h,
            -h,
            h,
            1,
            0,
            0,
            0,
            0,
            h,
            -h,
            -h,
            1,
            0,
            0,
            1,
            0,
            h,
            h,
            -h,
            1,
            0,
            0,
            1,
            1,
            h,
            -h,
            h,
            1,
            0,
            0,
            0,
            0,
            h,
            h,
            -h,
            1,
            0,
            0,
            1,
            1,
            h,
            h,
            h,
            1,
            0,
            0,
            0,
            1,
            // 底面 (0,-1,0)
            -h,
            -h,
            -h,
            0,
            -1,
            0,
            0,
            0,
            h,
            -h,
            -h,
            0,
            -1,
            0,
            1,
            0,
            h,
            -h,
            h,
            0,
            -1,
            0,
            1,
            1,
            -h,
            -h,
            -h,
            0,
            -1,
            0,
            0,
            0,
            h,
            -h,
            h,
            0,
            -1,
            0,
            1,
            1,
            -h,
            -h,
            h,
            0,
            -1,
            0,
            0,
            1,
            // 顶面 (0,1,0)
            -h,
            h,
            h,
            0,
            1,
            0,
            0,
            0,
            h,
            h,
            h,
            0,
            1,
            0,
            1,
            0,
            h,
            h,
            -h,
            0,
            1,
            0,
            1,
            1,
            -h,
            h,
            h,
            0,
            1,
            0,
            0,
            0,
            h,
            h,
            -h,
            0,
            1,
            0,
            1,
            1,
            -h,
            h,
            -h,
            0,
            1,
            0,
            0,
            1,
        };

        return createMeshFromData(vertices, sizeof(vertices), 36, nullptr, 0, false);
    }

    // ---- 创建全屏四边形（NDC 空间 [-1,1]） ----
    // 用于后处理、全屏特效
    MeshComponent createQuad()
    {
        float vertices[] = {
            // pos(3)      normal(3)    uv(2)
            -1,
            1,
            0,
            0,
            0,
            1,
            0,
            1,
            -1,
            -1,
            0,
            0,
            0,
            1,
            0,
            0,
            1,
            -1,
            0,
            0,
            0,
            1,
            1,
            0,

            -1,
            1,
            0,
            0,
            0,
            1,
            0,
            1,
            1,
            -1,
            0,
            0,
            0,
            1,
            1,
            0,
            1,
            1,
            0,
            0,
            0,
            1,
            1,
            1,
        };

        return createMeshFromData(vertices, sizeof(vertices), 6, nullptr, 0, false);
    }

    // ---- 创建平面 (XZ 平面) ----
    MeshComponent createPlane(float size = 10.0f)
    {
        float h = size * 0.5f;
        float vertices[] = {
            // 上表面法线指向 Y
            -h,
            0,
            -h,
            0,
            1,
            0,
            0,
            0,
            h,
            0,
            -h,
            0,
            1,
            0,
            1,
            0,
            h,
            0,
            h,
            0,
            1,
            0,
            1,
            1,
            -h,
            0,
            -h,
            0,
            1,
            0,
            0,
            0,
            h,
            0,
            h,
            0,
            1,
            0,
            1,
            1,
            -h,
            0,
            h,
            0,
            1,
            0,
            0,
            1,
        };

        return createMeshFromData(vertices, sizeof(vertices), 6, nullptr, 0, false);
    }

    // ---- 创建球体 ----
    // 使用 UV 球体（经线/纬线），sectorCount 越大越精细
    MeshComponent createSphere(float radius = 1.0f, int sectorCount = 36, int stackCount = 18)
    {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        for (int i = 0; i <= stackCount; ++i)
        {
            float stackAngle = M_PI / 2 - i * M_PI / stackCount; // 从 pi/2 到 -pi/2
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);

            for (int j = 0; j <= sectorCount; ++j)
            {
                float sectorAngle = j * 2 * M_PI / sectorCount;

                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                // 法线（单位球上法线 = 位置归一化）
                float nx = x / radius;
                float ny = y / radius;
                float nz = z / radius;

                // UV
                float u = (float)j / sectorCount;
                float v = (float)i / stackCount;

                vertices.insert(vertices.end(), {x, y, z, nx, ny, nz, u, v});
            }
        }

        // 生成索引（三角形带）
        for (int i = 0; i < stackCount; ++i)
        {
            int k1 = i * (sectorCount + 1);
            int k2 = k1 + sectorCount + 1;

            for (int j = 0; j < sectorCount; ++j)
            {
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stackCount - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
                ++k1;
                ++k2;
            }
        }

        return createMeshFromData(vertices.data(),
                                  vertices.size() * sizeof(float),
                                  static_cast<GLsizei>(vertices.size() / 8),
                                  indices.data(),
                                  static_cast<GLsizei>(indices.size()),
                                  true);
    }

    // 释放网格的 OpenGL 资源
    void destroyMesh(MeshComponent &mesh)
    {
        ensureInit(); // 确保 GL 函数已加载
        if (mesh.vao)
        {
            glDeleteVertexArrays(1, &mesh.vao);
            mesh.vao = 0;
        }
        if (mesh.vbo)
        {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
        }
        if (mesh.ebo)
        {
            glDeleteBuffers(1, &mesh.ebo);
            mesh.ebo = 0;
        }
        mesh.vertexCount = 0;
        mesh.indexCount = 0;
    }

private:
    // 从裸数据创建 VAO/VBO/EBO
    // 顶点格式: pos(3) + normal(3) + texcoord(2) = 8 floats
    MeshComponent createMeshFromData(const void *vertexData, size_t vertexDataSize,
                                     GLsizei vertexCount,
                                     const unsigned int *indexData, GLsizei indexCount,
                                     bool hasIndices)
    {
        ensureInit(); // 懒初始化 OpenGL 函数
        MeshComponent mesh;
        mesh.vertexCount = vertexCount;
        mesh.indexCount = indexCount;
        mesh.hasIndices = hasIndices;
        mesh.primitiveType = GL_TRIANGLES;
        int stride = 8 * sizeof(float); // pos(3)+norm(3)+uv(2)

        // 创建 VAO
        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);

        // 创建 VBO
        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize, vertexData, GL_STATIC_DRAW);

        // 创建 EBO（如果需要）
        if (hasIndices && indexData)
        {
            glGenBuffers(1, &mesh.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         indexCount * sizeof(unsigned int),
                         indexData, GL_STATIC_DRAW);
        }

        // 顶点属性布局
        // location 0: position (3 floats)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
        glEnableVertexAttribArray(0);

        // location 1: normal (3 floats)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // location 2: texcoord (2 floats)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // 解绑
        glBindVertexArray(0);

        return mesh;
    }

    bool m_initialized = false;
};

#endif // ENGINE_RESOURCES_MESH_FACTORY_H
