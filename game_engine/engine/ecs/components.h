#ifndef GAME_ENGINE_ECS_COMPONENTS_H
#define GAME_ENGINE_ECS_COMPONENTS_H

// ============================================================
// 所有 ECS 组件定义 — 纯数据结构，供 Flecs 使用
//
// 数学库：GLM (OpenGL Mathematics)，替代 Qt 的 QVector3D/QMatrix4x4
// GLM 的矩阵/向量 API 与 GLSL 高度一致，便于理解 Shader 端代码
// ============================================================

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QOpenGLFunctions_3_3_Core> // 仅用于 GLuint 类型，后续可替换

// ---- 变换组件 ----
// 每个可见实体必备：位置 / 旋转(欧拉角, 度) / 缩放
struct TransformComponent
{
    glm::vec3 position{0.0f, 0.0f, 0.0f}; // 位置
    glm::vec3 rotation{0.0f, 0.0f, 0.0f}; // 欧拉角（度），传给 Shader 前转弧度
    glm::vec3 scale{1.0f, 1.0f, 1.0f}; // 缩放

    // 构建模型矩阵：先缩放 → 再旋转(Z→Y→X) → 最后平移
    glm::mat4 getModelMatrix() const
    {
        glm::mat4 m(1.0f); // 单位阵
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }

    // 法线矩阵 = 模型矩阵的逆转置（处理非均匀缩放时法线方向修正）
    glm::mat3 getNormalMatrix() const
    {
        return glm::mat3(glm::transpose(glm::inverse(getModelMatrix())));
    }
};

// ---- 网格渲染组件 ----
// 持有 GPU 端的 VAO / VBO / EBO 资源句柄
struct MeshComponent
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei vertexCount = 0;
    GLsizei indexCount = 0;
    GLenum primitiveType = GL_TRIANGLES;
    bool hasIndices = false;

    char resourcePath[256] = ""; // 原始模型文件路径（如 .obj）

    bool isValid() const { return vao != 0; }
};

// ---- 材质组件 ----
// Phong 光照模型的材质参数 + 可选贴图
struct MaterialComponent
{
    glm::vec3 ambient{0.2f, 0.2f, 0.2f};
    glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;

    GLuint diffuseTexture = 0; // 0 = 无纹理
    GLuint specularTexture = 0;
    char shaderName[64] = "default"; // 绑定的着色器名
};

// ---- 光源类型 ----
enum class LightType : int
{
    Directional = 0, // 方向光（平行光，无衰减）
    Point = 1,       // 点光源（向四周发光，有距离衰减）
    Spot = 2         // 聚光灯（锥形光束，有角度 + 距离衰减）
};

// ---- 光源组件 ----
struct LightComponent
{
    LightType type = LightType::Point;
    glm::vec3 ambient{0.2f, 0.2f, 0.2f};
    glm::vec3 diffuse{0.5f, 0.5f, 0.5f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};

    // 衰减系数：1 / (constant + linear*d + quadratic*d²)
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    // 聚光灯锥角（cos 值）
    float cutOff = 0.91f;      // cos(内锥角), 约 24°
    float outerCutOff = 0.82f; // cos(外锥角), 约 35°
};

// ---- 摄像机组件 ----
struct CameraComponent
{
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fov = 45.0f; // 视场角（度）
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool active = true;

    CameraComponent() = default;
    CameraComponent(glm::vec3 pos, glm::vec3 f, glm::vec3 u,
                    float fv = 45.0f, float nr = 0.1f, float fr = 100.0f)
        : position(pos), front(f), up(u), fov(fv), nearPlane(nr), farPlane(fr) {}
};

// ---- 标签组件（实体名称） ----
// 注意：使用固定大小 char 数组而非 std::string，
//       因为 Flecs 用 memcpy 复制组件，std::string 内部指针会导致崩溃
struct TagComponent
{
    char name[64] = "Entity";
};

// ---- 天空盒组件 ----
struct SkyboxComponent
{
    GLuint cubemapTexture = 0;
};

#endif // GAME_ENGINE_ECS_COMPONENTS_H
