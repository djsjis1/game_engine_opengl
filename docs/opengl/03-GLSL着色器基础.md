# 03 · GLSL 着色器基础

> 着色器是跑在 GPU 上的小程序。顶点着色器处理每个顶点，片段着色器处理每个像素。

---

## 3.1 GLSL 是什么？

```
GLSL = OpenGL Shading Language

类比:
  C/C++  → 跑在 CPU 上，控制程序逻辑
  GLSL   → 跑在 GPU 上，控制渲染效果

特点:
  · 类 C 语法 (if/for/function)
  · 内置向量/矩阵类型 (vec3, mat4...)
  · 内置数学函数 (normalize, dot, cross, pow...)
  · 并行执行 (数百个核心同时处理顶点/像素)
```

---

## 3.2 两种着色器的分工

```
顶点着色器 (Vertex Shader)      片段着色器 (Fragment Shader)
───────────────────────        ──────────────────────────
执行次数: 每个顶点 1 次          执行次数: 每个像素 1 次
输入:     顶点属性 (位置/法线)   输入:     插值后的数据
输出:     gl_Position          输出:     fragColor (RGBA)

  三角形 3 个顶点                 三角形覆盖 10000 个像素
  → 顶点着色器执行 3 次            → 片段着色器执行 10000 次！
```

**性能要点**：片段着色器的执行次数远多于顶点着色器，所以复杂计算尽量放顶点着色器。

---

## 3.3 顶点着色器详解

### 最小顶点着色器

```glsl
#version 330 core                          // ① 版本声明 (必须第一行)

layout(location = 0) in vec3 aPos;         // ② 输入: 顶点属性 (location 与 C++ 端对应)

void main() {
    gl_Position = vec4(aPos, 1.0);         // ③ 输出: 裁剪空间坐标 (必须赋值)
}
```

### 本项目的顶点着色器 (default.vert)

```glsl
#version 330 core

// ===== 输入: 顶点属性 =====
layout(location = 0) in vec3 aPos;     // 位置 (location 0)
layout(location = 1) in vec3 aNormal;  // 法线 (location 1)

// ===== 输入: Uniform (全局变量) =====
uniform mat4 uModel;        // 模型矩阵 (物体自身变换)
uniform mat4 uView;         // 视图矩阵 (摄像机)
uniform mat4 uProjection;   // 投影矩阵 (透视/正交)
uniform mat3 uNormalMatrix; // 法线矩阵 (模型矩阵的逆转置)

// ===== 输出: 传给片段着色器 =====
out vec3 vFragPos;   // 世界空间位置 (用于光照计算)
out vec3 vNormal;    // 世界空间法线

void main() {
    // ① 把顶点从模型空间变换到世界空间
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = vec3(worldPos);

    // ② 法线变换 (法线矩阵 = 模型矩阵的逆转置)
    vNormal = normalize(uNormalMatrix * aNormal);

    // ③ 世界空间 → 裁剪空间 (必须赋值给 gl_Position)
    gl_Position = uProjection * uView * worldPos;
}
```

### 关键解读

```glsl
vec4 worldPos = uModel * vec4(aPos, 1.0);
//                                 ↑
//  vec4(aPos, 1.0) 把 (x,y,z) 扩展为 (x,y,z,1)
//  w=1 表示这是一个"点" (位置)
//  w=0 表示这是一个"方向" (用于法线、光线方向)
```

---

## 3.4 片段着色器详解

### 最小片段着色器

```glsl
#version 330 core

out vec4 fragColor;                    // 输出: 像素颜色 (RGBA)

void main() {
    fragColor = vec4(1.0, 0.5, 0.2, 1.0);  // 橙色
}
```

### 本项目的片段着色器 (default.frag)

```glsl
#version 330 core

// ===== 输入: 从顶点着色器传来的插值数据 =====
in vec3 vFragPos;   // 当前像素的世界位置 (已插值)
in vec3 vNormal;    // 当前像素的法线 (已插值)

// ===== 输入: Uniform (光源 + 材质 + 摄像机) =====
struct DirLight {
    vec3 direction;  // 光源方向
    vec3 ambient;    // 环境光颜色
    vec3 diffuse;    // 漫反射颜色
    vec3 specular;   // 镜面反射颜色
};
uniform DirLight uLight;

struct Material {
    vec3 ambient;    // 材质环境光反射率
    vec3 diffuse;    // 材质漫反射反射率
    vec3 specular;   // 材质镜面反射反射率
    float shininess; // 高光锐度
};
uniform Material uMaterial;

uniform vec3 uCameraPos;  // 摄像机位置

// ===== 输出 =====
out vec4 fragColor;

void main() {
    // ① 环境光: 所有地方都一样 (防止完全黑)
    vec3 ambient = uLight.ambient * uMaterial.ambient;

    // ② 漫反射 (Lambert 模型): 面朝光源越正，越亮
    vec3 norm = normalize(vNormal);               // 归一化法线
    vec3 lightDir = normalize(-uLight.direction); // 指向光源的方向
    float diff = max(dot(norm, lightDir), 0.0);   // 夹角余弦
    vec3 diffuse = uLight.diffuse * diff * uMaterial.diffuse;

    // ③ 镜面反射 (Blinn-Phong): 高光
    vec3 viewDir = normalize(uCameraPos - vFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  // 半角向量
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = uLight.specular * spec * uMaterial.specular;

    // ④ 最终颜色 = 环境光 + 漫反射 + 镜面反射
    vec3 result = ambient + diffuse + specular;
    fragColor = vec4(result, 1.0);
}
```

---

## 3.5 C++ 端如何编译、链接着色器

```c
// ① 读取 .vert / .frag 文件内容
std::string vertSrc = readFile("default.vert");
std::string fragSrc = readFile("default.frag");

// ② 编译顶点着色器
GLuint vs = glCreateShader(GL_VERTEX_SHADER);
const char *src = vertSrc.c_str();
glShaderSource(vs, 1, &src, nullptr);
glCompileShader(vs);
// 检查编译错误:
GLint ok;
glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
if (!ok) { /* 打印错误日志 */ }

// ③ 编译片段着色器
GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
// ... 同上 ...

// ④ 链接着色器程序
GLuint program = glCreateProgram();
glAttachShader(program, vs);
glAttachShader(program, fs);
glLinkProgram(program);
glGetProgramiv(program, GL_LINK_STATUS, &ok);

// ⑤ 清理 (着色器已经链接进程序，可以删了)
glDeleteShader(vs);
glDeleteShader(fs);

// ⑥ 使用
glUseProgram(program);
```

### 项目中的 Shader 类

本项目封装在 `System/Shader.h` 中：

```c
class Shader : protected QOpenGLFunctions_3_3_Core {
public:
    bool load(const std::string &vertPath, const std::string &fragPath);
    void use();              // glUseProgram
    bool isValid() const;    // program != 0
    
    void setMat4(const std::string &name, const glm::mat4 &m);
    void setVec3(const std::string &name, const glm::vec3 &v);
    void setFloat(const std::string &name, float v);
    void setInt(const std::string &name, int v);
};
```

---

## 3.6 GLSL 常用内置函数

| 函数 | 作用 | 示例 |
|------|------|------|
| `dot(a, b)` | 点积 (判夹角) | `max(dot(N, L), 0.0)` |
| `cross(a, b)` | 叉积 (求法线) | `cross(edge1, edge2)` |
| `normalize(v)` | 归一化 | `normalize(lightDir)` |
| `pow(x, n)` | 幂运算 | `pow(spec, 32)` |
| `max(a, b)` | 取最大值 | `max(x, 0.0)` |
| `mix(a, b, t)` | 线性插值 | `mix(red, blue, 0.5)` |
| `reflect(I, N)` | 反射向量 | 镜面反射计算 |
| `texture(tex, uv)` | 采样纹理 | `texture(diffuseMap, uv)` |

---

## 3.7 GLSL 数据类型速查

```glsl
// 标量
float f = 3.14;
int   i = 42;
bool  b = true;

// 向量 (常用)
vec2  v2 = vec2(1.0, 2.0);       // 2D
vec3  v3 = vec3(1.0, 2.0, 3.0);  // 3D (颜色、位置、法线)
vec4  v4 = vec4(1.0, 2.0, 3.0, 1.0); // 4D (齐次坐标、RGBA)

// 访问分量 (多种方式，结果一样)
vec3 color;
color.rgb   // 颜色风格
color.xyz   // 坐标风格
color[0]    // 数组风格

// 矩阵
mat4 m = mat4(1.0);  // 4×4 单位矩阵
mat3 m3;             // 3×3
```

---

## 3.8 本项目使用两条渲染管线

```
default.vert + default.frag  →  渲染 3D 物体 (带光照)
  输入:  位置(loc=0) + 法线(loc=1)
  Uniform: uModel, uView, uProjection, uNormalMatrix,
           uLight, uMaterial, uCameraPos

line.vert + line.frag       →  渲染 Gizmo 辅助线
  输入:  位置(loc=0)
  Uniform: uModel, uView, uProjection, uColor
```

---

## 3.9 总结

```
着色器 = GPU 端的程序，用 GLSL 编写

顶点着色器:
  · 处理每个顶点
  · 输入 = 顶点属性 (in)
  · 输出 = gl_Position (必须) + 自定义 (out)

片段着色器:
  · 处理每个像素
  · 输入 = 从顶点着色器插值来的数据 (in) + Uniform
  · 输出 = fragColor (out)

C++ 端:
  glCreateShader → glShaderSource → glCompileShader
  → glCreateProgram → glAttachShader → glLinkProgram
  → glUseProgram → glUniform* → glDraw*
```

---

**下一篇**: [04-Uniform与顶点属性](./04-Uniform与顶点属性.md) —— C++ 怎么给着色器传数据？
