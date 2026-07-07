#ifndef GAME_ENGINE_RENDER_MESH_FACTORY_H
#define GAME_ENGINE_RENDER_MESH_FACTORY_H

#include <QOpenGLFunctions_3_3_Core>

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

#include "../ecs/components.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// MeshFactory — 程序化几何体 + 简单 OBJ 加载
//
// 所有几何体顶点格式: pos(3) + normal(3) + texcoord(2) = 8 floats
// ============================================================
class MeshFactory : protected QOpenGLFunctions_3_3_Core
{
public:
    MeshFactory() = default;

    void ensureInit()
    {
        if (!m_init)
        {
            initializeOpenGLFunctions();
            m_init = true;
        }
    }

    MeshComponent createCube(float size = 1.0f);
    MeshComponent createQuad();
    MeshComponent createPlane(float size = 10.0f);
    MeshComponent createSphere(float radius = 1.0f, int sectors = 36, int stacks = 18);

    // 从 OBJ 文件加载（支持 v/vn/vt/f）
    MeshComponent loadOBJ(const std::string& filePath);

    void destroyMesh(MeshComponent& mesh)
    {
        ensureInit();
        if (mesh.vao) { glDeleteVertexArrays(1, &mesh.vao); mesh.vao = 0; }
        if (mesh.vbo) { glDeleteBuffers(1, &mesh.vbo);      mesh.vbo = 0; }
        if (mesh.ebo) { glDeleteBuffers(1, &mesh.ebo);      mesh.ebo = 0; }
        mesh.vertexCount = mesh.indexCount = 0;
    }

private:
    MeshComponent createMeshFromData(const void* vData, size_t vSize, GLsizei vCount,
                                     const unsigned int* iData, GLsizei iCount, bool hasIdx);
    bool m_init = false;
};

// ============================================================
// 内联实现（header-only 引擎核心）
// ============================================================

// ---- 立方体 (12 个三角形, 36 顶点, 每个面单独法线) ----
inline MeshComponent MeshFactory::createCube(float size)
{
    float h = size * 0.5f;
    float verts[] = {
        // +Z 面
        -h,-h, h,  0,0,1, 0,0,   h,-h, h,  0,0,1, 1,0,   h, h, h,  0,0,1, 1,1,
        -h,-h, h,  0,0,1, 0,0,   h, h, h,  0,0,1, 1,1,  -h, h, h,  0,0,1, 0,1,
        // -Z 面
         h,-h,-h,  0,0,-1,0,0,  -h,-h,-h,  0,0,-1,1,0,  -h, h,-h,  0,0,-1,1,1,
         h,-h,-h,  0,0,-1,0,0,  -h, h,-h,  0,0,-1,1,1,   h, h,-h,  0,0,-1,0,1,
        // -X 面
        -h,-h,-h, -1,0,0, 0,0,  -h,-h, h, -1,0,0, 1,0,  -h, h, h, -1,0,0, 1,1,
        -h,-h,-h, -1,0,0, 0,0,  -h, h, h, -1,0,0, 1,1,  -h, h,-h, -1,0,0, 0,1,
        // +X 面
         h,-h, h,  1,0,0, 0,0,   h,-h,-h,  1,0,0, 1,0,   h, h,-h,  1,0,0, 1,1,
         h,-h, h,  1,0,0, 0,0,   h, h,-h,  1,0,0, 1,1,   h, h, h,  1,0,0, 0,1,
        // -Y 面
        -h,-h,-h,  0,-1,0,0,0,   h,-h,-h,  0,-1,0,1,0,   h,-h, h,  0,-1,0,1,1,
        -h,-h,-h,  0,-1,0,0,0,   h,-h, h,  0,-1,0,1,1,  -h,-h, h,  0,-1,0,0,1,
        // +Y 面
        -h, h, h,  0,1,0, 0,0,   h, h, h,  0,1,0, 1,0,   h, h,-h,  0,1,0, 1,1,
        -h, h, h,  0,1,0, 0,0,   h, h,-h,  0,1,0, 1,1,  -h, h,-h,  0,1,0, 0,1,
    };
    return createMeshFromData(verts, sizeof(verts), 36, nullptr, 0, false);
}

// ---- 全屏四边形 (NDC 空间, 用于后处理) ----
inline MeshComponent MeshFactory::createQuad()
{
    float v[] = {
        -1,1,0, 0,0,1, 0,1,  -1,-1,0, 0,0,1, 0,0,  1,-1,0, 0,0,1, 1,0,
        -1,1,0, 0,0,1, 0,1,   1,-1,0, 0,0,1, 1,0,  1, 1,0, 0,0,1, 1,1,
    };
    return createMeshFromData(v, sizeof(v), 6, nullptr, 0, false);
}

// ---- 平面 (XZ 平面, 法线朝上) ----
inline MeshComponent MeshFactory::createPlane(float size)
{
    float h = size * 0.5f;
    float v[] = {
        -h,0,-h, 0,1,0, 0,0,   h,0,-h, 0,1,0, 1,0,   h,0, h, 0,1,0, 1,1,
        -h,0,-h, 0,1,0, 0,0,   h,0, h, 0,1,0, 1,1,  -h,0, h, 0,1,0, 0,1,
    };
    return createMeshFromData(v, sizeof(v), 6, nullptr, 0, false);
}

// ---- 球体 (经纬球, 带索引缓冲) ----
inline MeshComponent MeshFactory::createSphere(float radius, int sectors, int stacks)
{
    std::vector<float> verts;
    std::vector<unsigned int> idx;

    for (int i = 0; i <= stacks; ++i)
    {
        float sa = M_PI / 2.0f - static_cast<float>(i) * M_PI / stacks;
        float xy = radius * cosf(sa);
        float z  = radius * sinf(sa);

        for (int j = 0; j <= sectors; ++j)
        {
            float sA = static_cast<float>(j) * 2.0f * M_PI / sectors;
            float x  = xy * cosf(sA);
            float y  = xy * sinf(sA);

            verts.insert(verts.end(), {
                x, y, z,                              // pos
                x / radius, y / radius, z / radius,   // normal
                static_cast<float>(j) / sectors,      // u
                static_cast<float>(i) / stacks        // v
            });
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j)
        {
            if (i != 0)
            {
                idx.push_back(k1);
                idx.push_back(k2);
                idx.push_back(k1 + 1);
            }
            if (i != stacks - 1)
            {
                idx.push_back(k1 + 1);
                idx.push_back(k2);
                idx.push_back(k2 + 1);
            }
            ++k1; ++k2;
        }
    }

    return createMeshFromData(verts.data(), verts.size() * sizeof(float),
        static_cast<GLsizei>(verts.size() / 8),
        idx.data(), static_cast<GLsizei>(idx.size()), true);
}

// ---- OBJ 加载器（简单实现, 仅支持三角形面） ----
inline MeshComponent MeshFactory::loadOBJ(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return MeshComponent();

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<float> vertexData;
    std::vector<unsigned int> indices;
    unsigned int idx = 0;

    std::string line;
    while (std::getline(file, line))
    {
        // 去除首尾空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v")
        {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back({ x, y, z });
        }
        else if (prefix == "vn")
        {
            float x, y, z;
            iss >> x >> y >> z;
            normals.push_back({ x, y, z });
        }
        else if (prefix == "vt")
        {
            float u, v;
            iss >> u >> v;
            uvs.push_back({ u, v });
        }
        else if (prefix == "f")
        {
            // 格式：f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
            std::string v1, v2, v3;
            iss >> v1 >> v2 >> v3;

            auto parseFaceVertex = [&](const std::string& s, int& pi, int& ti, int& ni)
            {
                pi = ti = ni = -1;
                size_t s1 = s.find('/');
                pi = std::stoi(s.substr(0, s1)) - 1;

                if (s1 != std::string::npos)
                {
                    size_t s2 = s.find('/', s1 + 1);
                    std::string tPart = s.substr(s1 + 1, s2 - s1 - 1);
                    if (!tPart.empty()) ti = std::stoi(tPart) - 1;

                    if (s2 != std::string::npos)
                    {
                        std::string nPart = s.substr(s2 + 1);
                        if (!nPart.empty()) ni = std::stoi(nPart) - 1;
                    }
                }
            };

            int pi, ti, ni;
            for (const auto& v : { v1, v2, v3 })
            {
                parseFaceVertex(v, pi, ti, ni);

                glm::vec3 p = (pi >= 0 && pi < static_cast<int>(positions.size())) ? positions[pi] : glm::vec3(0.0f);
                glm::vec3 n = (ni >= 0 && ni < static_cast<int>(normals.size()))   ? normals[ni]   : glm::vec3(0.0f, 1.0f, 0.0f);
                glm::vec2 t = (ti >= 0 && ti < static_cast<int>(uvs.size()))        ? uvs[ti]       : glm::vec2(0.0f);

                vertexData.insert(vertexData.end(), {
                    p.x, p.y, p.z,
                    n.x, n.y, n.z,
                    t.x, t.y
                });
                indices.push_back(idx++);
            }
        }
    }

    return createMeshFromData(vertexData.data(), vertexData.size() * sizeof(float),
        static_cast<GLsizei>(vertexData.size() / 8),
        indices.data(), static_cast<GLsizei>(indices.size()), true);
}

// ---- 通用：将顶点数据上传到 GPU ----
inline MeshComponent MeshFactory::createMeshFromData(
    const void* vData, size_t vSize, GLsizei vCount,
    const unsigned int* iData, GLsizei iCount, bool hasIdx)
{
    ensureInit();
    MeshComponent m;
    m.vertexCount = vCount;
    m.indexCount  = iCount;
    m.hasIndices  = hasIdx;

    constexpr int stride = 8 * static_cast<int>(sizeof(float));

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    // VBO
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vSize), vData, GL_STATIC_DRAW);

    // EBO (可选)
    if (hasIdx && iData)
    {
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(iCount * sizeof(unsigned int)), iData, GL_STATIC_DRAW);
    }

    // 顶点属性布局: pos(3) + normal(3) + texcoord(2)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return m;
}

#endif // GAME_ENGINE_RENDER_MESH_FACTORY_H
#ifndef GAME_ENGINE_RENDER_MESH_FACTORY_H
#define GAME_ENGINE_RENDER_MESH_FACTORY_H

#include <QOpenGLFunctions_3_3_Core>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <cmath>
#include "../ecs/components.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// MeshFactory: 程序化几何体 + 简单 OBJ 加载
// 所有几何体顶点格式: pos(3) + normal(3) + texcoord(2) = 8 floats
// ============================================================
class MeshFactory : protected QOpenGLFunctions_3_3_Core
{
public:
    MeshFactory() = default;

    void ensureInit()
    {
        if (!m_init)
        {
            initializeOpenGLFunctions();
            m_init = true;
        }
    }

    // Cube, Quad, Plane, Sphere — 与原有实现相同，省略重复代码
    MeshComponent createCube(float size = 1.0f);
    MeshComponent createQuad();
    MeshComponent createPlane(float size = 10.0f);
    MeshComponent createSphere(float radius = 1.0f, int sectors = 36, int stacks = 18);

    // 从 OBJ 文件加载（简单实现，支持顶点位置+法线+UV）
    MeshComponent loadOBJ(const QString &filePath);

    void destroyMesh(MeshComponent &mesh)
    {
        ensureInit();
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
        mesh.vertexCount = mesh.indexCount = 0;
    }

private:
    MeshComponent createMeshFromData(const void *vData, size_t vSize, GLsizei vCount,
                                     const unsigned int *iData, GLsizei iCount, bool hasIdx);
    bool m_init = false;
};

// ---- 实现 ----
inline MeshComponent MeshFactory::createCube(float size)
{
    float h = size * 0.5f;
    float verts[] = {
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
    return createMeshFromData(verts, sizeof(verts), 36, nullptr, 0, false);
}

inline MeshComponent MeshFactory::createQuad()
{
    float v[] = {-1, 1, 0, 0, 0, 1, 0, 1, -1, -1, 0, 0, 0, 1, 0, 0, 1, -1, 0, 0, 0, 1, 1, 0,
                 -1, 1, 0, 0, 0, 1, 0, 1, 1, -1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1};
    return createMeshFromData(v, sizeof(v), 6, nullptr, 0, false);
}

inline MeshComponent MeshFactory::createPlane(float size)
{
    float h = size * 0.5f;
    float v[] = {-h, 0, -h, 0, 1, 0, 0, 0, h, 0, -h, 0, 1, 0, 1, 0, h, 0, h, 0, 1, 0, 1, 1,
                 -h, 0, -h, 0, 1, 0, 0, 0, h, 0, h, 0, 1, 0, 1, 1, -h, 0, h, 0, 1, 0, 0, 1};
    return createMeshFromData(v, sizeof(v), 6, nullptr, 0, false);
}

inline MeshComponent MeshFactory::createSphere(float radius, int sectors, int stacks)
{
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    for (int i = 0; i <= stacks; ++i)
    {
        float sa = M_PI / 2 - i * M_PI / stacks;
        float xy = radius * cosf(sa), z = radius * sinf(sa);
        for (int j = 0; j <= sectors; ++j)
        {
            float sA = j * 2 * M_PI / sectors;
            float x = xy * cosf(sA), y = xy * sinf(sA);
            verts.insert(verts.end(), {x, y, z, x / radius, y / radius, z / radius, (float)j / sectors, (float)i / stacks});
        }
    }
    for (int i = 0; i < stacks; ++i)
    {
        int k1 = i * (sectors + 1), k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j)
        {
            if (i != 0)
            {
                idx.push_back(k1);
                idx.push_back(k2);
                idx.push_back(k1 + 1);
            }
            if (i != stacks - 1)
            {
                idx.push_back(k1 + 1);
                idx.push_back(k2);
                idx.push_back(k2 + 1);
            }
            ++k1;
            ++k2;
        }
    }
    return createMeshFromData(verts.data(), verts.size() * sizeof(float),
                              (GLsizei)(verts.size() / 8), idx.data(), (GLsizei)idx.size(), true);
}

inline MeshComponent MeshFactory::loadOBJ(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return MeshComponent();

    std::vector<QVector3D> positions, normals;
    std::vector<QVector2D> uvs;
    std::vector<float> vertexData;
    std::vector<unsigned int> indices;
    unsigned int idx = 0;

    QTextStream in(&file);
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.startsWith("v "))
        {
            auto parts = line.split(' ', Qt::SkipEmptyParts);
            positions.push_back({parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()});
        }
        else if (line.startsWith("vn "))
        {
            auto parts = line.split(' ', Qt::SkipEmptyParts);
            normals.push_back({parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()});
        }
        else if (line.startsWith("vt "))
        {
            auto parts = line.split(' ', Qt::SkipEmptyParts);
            uvs.push_back({parts[1].toFloat(), parts[2].toFloat()});
        }
        else if (line.startsWith("f "))
        {
            auto parts = line.split(' ', Qt::SkipEmptyParts);
            for (int i = 1; i <= 3; ++i)
            {
                auto fParts = parts[i].split('/');
                int pi = fParts[0].toInt() - 1;
                int ti = fParts.size() > 1 && !fParts[1].isEmpty() ? fParts[1].toInt() - 1 : -1;
                int ni = fParts.size() > 2 && !fParts[2].isEmpty() ? fParts[2].toInt() - 1 : -1;

                QVector3D p = positions[pi];
                QVector3D n = ni >= 0 ? normals[ni] : QVector3D(0, 1, 0);
                QVector2D uv = ti >= 0 ? uvs[ti] : QVector2D(0, 0);

                vertexData.insert(vertexData.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), uv.x(), uv.y()});
                indices.push_back(idx++);
            }
        }
    }

    return createMeshFromData(vertexData.data(), vertexData.size() * sizeof(float),
                              (GLsizei)(vertexData.size() / 8), indices.data(), (GLsizei)indices.size(), true);
}

inline MeshComponent MeshFactory::createMeshFromData(const void *vData, size_t vSize,
                                                     GLsizei vCount, const unsigned int *iData, GLsizei iCount, bool hasIdx)
{
    ensureInit();
    MeshComponent m;
    m.vertexCount = vCount;
    m.indexCount = iCount;
    m.hasIndices = hasIdx;
    int stride = 8 * sizeof(float);

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vSize, vData, GL_STATIC_DRAW);

    if (hasIdx && iData)
    {
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, iCount * sizeof(unsigned), iData, GL_STATIC_DRAW);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return m;
}

#endif // GAME_ENGINE_RENDER_MESH_FACTORY_H
