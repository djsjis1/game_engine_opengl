# Qt + OpenGL 学习手册

> 本手册以 **Qt 5 + OpenGL 3.3 Core Profile** 为基线，按课程顺序梳理核心知识点。
> 每个章节都给出"概念 → 关键 API → 代码片段 → 易错点"四段式说明，方便对照本仓库代码学习。

---

## 目录

1. [图形管线总览](#1-图形管线总览)
2. [Qt OpenGL 集成](#2-qt-opengl-集成)
3. [缓冲对象：VBO / VAO / EBO](#3-缓冲对象vbo--vao--ebo)
4. [着色器（GLSL）](#4-着色器glsl)
5. [纹理](#5-纹理)
6. [坐标系统与变换](#6-坐标系统与变换)
7. [摄像机](#7-摄像机)
8. [光照与材质](#8-光照与材质)
9. [投光物 (Light Casters)](#9-投光物-light-casters)
10. [多光源 (Multiple Lights)](#10-多光源-multiple-lights)
11. [光照贴图 (Lighting Maps)](#11-光照贴图-lighting-maps)
12. [深度与混合](#12-深度与混合)
13. [资源管理与生命周期](#13-资源管理与生命周期)

---

## 1. 图形管线总览

OpenGL 把"3D 数据 → 屏幕像素"的过程拆成 **可编程 + 固定** 的若干阶段：

```
顶点数据 (CPU)
  └─► [VBO/EBO/VAO]
       └─► 顶点着色器 VS  ← 每个顶点跑一次
            └─► 图元装配 (点/线/三角形)
                 └─► [可选] 几何着色器 GS
                      └─► 光栅化 (生成片段/像素候选)
                           └─► 片段着色器 FS  ← 每个片段跑一次
                                └─► 测试 (深度/模板/裁剪) → 混合 → 帧缓冲
```

**必须实现** 的两个阶段：**顶点着色器** 与 **片段着色器**。

---

## 2. Qt OpenGL 集成

### 2.1 三件套类

| 类 | 作用 |
|---|---|
| `QOpenGLWidget`              | 提供窗口 + OpenGL 上下文，是大部分项目的起点 |
| `QOpenGLFunctions_3_3_Core`  | 函数指针自动加载（替代 GLEW/GLAD），调用 `initializeOpenGLFunctions()` 后即可用 |
| `QSurfaceFormat`             | 在 `main()` 里设定 OpenGL 版本 / Profile / 深度位数 |

### 2.2 初始化模板

```cpp
// main.cpp
QSurfaceFormat fmt;
fmt.setVersion(3, 3);                       // OpenGL 3.3
fmt.setProfile(QSurfaceFormat::CoreProfile);// 核心模式（淘汰旧 API）
fmt.setDepthBufferSize(24);                 // 24 位深度缓冲
QSurfaceFormat::setDefaultFormat(fmt);
```

### 2.3 三个生命周期函数

```cpp
class MyWidget : public QOpenGLWidget,
                 protected QOpenGLFunctions_3_3_Core
{
protected:
    void initializeGL() override;       // 创建 GL 资源，仅调一次
    void resizeGL(int w, int h) override;// 视口/投影矩阵更新
    void paintGL() override;            // 每帧绘制
};
```

> ⚠️ 任何 `gl*` 函数 **必须** 在拿到上下文后调用，第一行总是 `initializeOpenGLFunctions();`。

### 2.4 触发重绘

* 主动重绘：`update()`（异步，下一轮事件循环 paintGL）
* 立即重绘：`repaint()`（很少用）
* 周期性渲染：`QTimer` + `connect(...&QTimer::timeout, this, [this]{ update(); })`

---

## 3. 缓冲对象：VBO / VAO / EBO

| 概念 | 全称 | 解决什么 |
|---|---|---|
| VBO | Vertex Buffer Object   | 把顶点数据放到 GPU |
| EBO | Element Buffer Object  | 用索引复用顶点（节省显存） |
| VAO | Vertex Array Object    | 记录 VBO/EBO 的绑定与属性指针，**一次配置，反复使用** |

### 3.1 标准创建顺序

```cpp
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);                                    // 绑定 VAO

glGenBuffers(1, &vbo);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

glGenBuffers(1, &ebo);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

// 顶点属性：location, 分量数, 类型, 是否归一化, 步长, 偏移
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
glEnableVertexAttribArray(1);

glBindVertexArray(0); // 解绑（可选）
```

### 3.2 绘制

```cpp
glBindVertexArray(vao);
glDrawArrays(GL_TRIANGLES, 0, 3);              // 直接画
// 或
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // 索引绘制
```

### 3.3 易错点

* `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,...)` 必须在 **VAO 绑定之后** 执行，否则 EBO 不会被 VAO 记录。
* `glVertexAttribPointer` 的 stride 单位是字节，记得用 `sizeof(float)`。
* Core Profile 下没有默认 VAO，**必须显式创建并绑定**，否则什么都画不出。

---

## 4. 着色器（GLSL）

### 4.1 最简顶点 / 片段着色器

```glsl
// vertex.vert
#version 330 core
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); }
```

```glsl
// fragment.frag
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.5, 0.2, 1.0); }
```

### 4.2 编译 + 链接流程

```cpp
GLuint vs = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vs, 1, &src, nullptr);
glCompileShader(vs);
// glGetShaderiv(vs, GL_COMPILE_STATUS, ...) 检查错误

GLuint program = glCreateProgram();
glAttachShader(program, vs);
glAttachShader(program, fs);
glLinkProgram(program);
// 检查 GL_LINK_STATUS

glDeleteShader(vs); glDeleteShader(fs); // 链接完即可释放
```

> 项目中 [shader.h](../lesson/lesson03_shader_uniform/shader.h) 已封装这一过程。

### 4.3 Uniform 变量

* 全部 fragment / vertex shader 共享、**全局 + 只读**。
* 设值前必须 `glUseProgram(program)`。
* 拿位置：`glGetUniformLocation(program, "name")`，找不到返回 -1。

```cpp
shader.use();
shader.setFloat("uTime", t);
shader.setVec3 ("uLightPos", 1, 2, 3);
shader.setMat4 ("uMVP", mvp.constData()); // QMatrix4x4 → const float*
```

### 4.4 顶点 → 片段的传递（in/out）

```glsl
// VS
out vec3 vColor;
void main() { vColor = aColor; ... }

// FS
in vec3 vColor;
void main() { FragColor = vec4(vColor, 1.0); }
```

光栅化阶段会对 `vColor` 自动做 **重心坐标插值**。

---

## 5. 纹理

### 5.1 创建并上传数据

```cpp
GLuint tex;
glGenTextures(1, &tex);
glBindTexture(GL_TEXTURE_2D, tex);

// 环绕：REPEAT / MIRRORED_REPEAT / CLAMP_TO_EDGE / CLAMP_TO_BORDER
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
// 过滤：NEAREST（像素风）/ LINEAR（平滑）
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             img.width(), img.height(), 0,
             GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
glGenerateMipmap(GL_TEXTURE_2D);
```

### 5.2 用 Qt 的 QImage 加载

```cpp
QImage img(":/textures/wall.jpg");        // 或 QImage("D:/path/wall.jpg")
img = img.convertToFormat(QImage::Format_RGBA8888)
        .mirrored(false, true);          // ⚠️ OpenGL Y 轴在底部，需要垂直翻转
```

### 5.3 纹理单元（多张纹理同时用）

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texA);
shader.setInt("uTexA", 0);   // 0 号单元

glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, texB);
shader.setInt("uTexB", 1);
```

```glsl
uniform sampler2D uTexA;
uniform sampler2D uTexB;
FragColor = mix(texture(uTexA, uv), texture(uTexB, uv), 0.3);
```

---

## 6. 坐标系统与变换

### 6.1 五个坐标系

```
本地空间 ──Model──► 世界空间 ──View──► 观察空间 ──Projection──► 裁剪空间 ──/w──► NDC
```

最终 `gl_Position` 必须落在裁剪空间，OpenGL 会自动做透视除法到 NDC `[-1,1]^3`。

### 6.2 Qt 的矩阵库

`QMatrix4x4` 是行优先存储但提供 OpenGL 兼容接口（`constData()` 返回列优先 16 个 float）。

```cpp
QMatrix4x4 model;
model.translate(1, 0, 0);
model.rotate(45.0f, 0, 1, 0);   // 角度，绕 Y 轴
model.scale(2.0f);

QMatrix4x4 view;
view.lookAt(eye, center, up);

QMatrix4x4 projection;
projection.perspective(45.0f, float(w)/h, 0.1f, 100.0f);
```

### 6.3 顶点着色器中相乘

```glsl
gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
```

> 顺序口诀：**右乘先生效**。`M * v` 中 `v` 先被 `M` 变换。
> 写法上从右往左：先 Model，再 View，最后 Projection。

### 6.4 法线变换

模型矩阵含 **非均匀缩放** 时，法线必须用 `transpose(inverse(mat3(model)))` 变换；
若只有平移+旋转+均匀缩放，可简化为 `mat3(model) * normal`。

---

## 7. 摄像机

### 7.1 摄像机三向量

```
front  // 看向的方向
up     // 头顶方向
right  // 右手方向 = normalize(cross(front, worldUp))
```

### 7.2 欧拉角转 front

```cpp
front.x = cos(yaw) * cos(pitch);
front.y = sin(pitch);
front.z = sin(yaw) * cos(pitch);
front.normalize();
```

* `yaw`（偏航）：左右转头，鼠标左右移动
* `pitch`（俯仰）：上下点头，鼠标上下移动，**限制在 ±89°** 防翻转

### 7.3 deltaTime（帧间时间差）

不同帧率下保持移动速度一致：

```cpp
float dt = (now - lastFrameTime);
cameraPos += movementSpeed * dt * front;
```

### 7.4 鼠标无限旋转的小技巧

```cpp
QCursor::setPos(mapToGlobal(rect().center()));  // 每帧把光标拉回中心
setCursor(Qt::BlankCursor);                     // 隐藏光标
```

---

## 8. 光照与材质

### 8.1 Phong 光照模型（最常用）

```
最终颜色 = ambient + diffuse + specular
```

| 分量 | 公式 | 直觉 |
|---|---|---|
| Ambient  | `Ka * lightColor`                                              | 环境基础亮度 |
| Diffuse  | `max(dot(N, L), 0) * Kd * lightColor`                          | 朗伯漫反射，与法线-光线夹角相关 |
| Specular | `pow(max(dot(V, R), 0), shininess) * Ks * lightColor`          | 高光，与视线-反射方向相关 |

`R = reflect(-L, N)`，`L`/`V`/`N` 都是 **单位向量**。

### 8.2 材质 struct

```glsl
struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};
uniform Material uMaterial;
```

CPU 端需要 **逐字段** 传递，因为 OpenGL 的 uniform 不支持整体设置 struct：

```cpp
shader.setVec3 ("uMaterial.ambient",  mat.ambient);
shader.setVec3 ("uMaterial.diffuse",  mat.diffuse);
shader.setVec3 ("uMaterial.specular", mat.specular);
shader.setFloat("uMaterial.shininess",mat.shininess);
```

### 8.3 经典材质数值

| 名称 | shininess |
|---|---|
| 翡翠 (Emerald) | 76.8  |
| 黄金 (Gold)    | 51.2  |
| 白银 (Silver)  | 51.2  |
| 青铜 (Bronze)  | 25.6  |
| 珍珠 (Pearl)   | 11.264|

完整数值见 [lesson10_material/openglwidget.cpp](../lesson/lesson10_material/openglwidget.cpp)。

---

## 9. 投光物 (Light Casters)

对应课程：**lesson12_light_casters**

### 9.1 三种光源类型

| 类型 | 特点 | 关键参数 |
|---|---|---|
| 方向光 (Directional) | 平行光，无距离衰减，类似太阳 | `direction` |
| 点光源 (Point) | 向所有方向发光，有衰减 | `position` + `constant/linear/quadratic` |
| 聚光灯 (Spot)   | 锥形光束，点光源 + 方向 + 切角 | 以上 + `direction` + `cutOff`/`outerCutOff` |

### 9.2 衰减公式

```
attenuation = 1.0 / (constant + linear * d + quadratic * d²)
```

常用经验值：

| 距离范围 | constant | linear | quadratic |
|---|---|---|---|
| 7   | 1.0 | 0.7  | 1.8  |
| 13  | 1.0 | 0.35 | 0.44 |
| 20  | 1.0 | 0.22 | 0.20 |
| 50  | 1.0 | 0.09 | 0.032 |

### 9.3 聚光灯边缘软化

```glsl
float theta = dot(lightDir, normalize(-light.direction));
float epsilon = cutOff - outerCutOff;
float intensity = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);
```

* `cutOff` = cos(内锥角)，`outerCutOff` = cos(外锥角)
* `cutOff` > `outerCutOff`：内锥全亮，外锥渐变到 0

---

## 10. 多光源 (Multiple Lights)

对应课程：**lesson13_multiple_lights**

### 10.1 设计思路

把光照计算封装为**函数**，对每种光源分别调用，最后累加：

```glsl
vec3 calcDirLight(DirLight l, vec3 n, vec3 v);     // 方向光
vec3 calcPointLight(PointLight l, vec3 n, vec3 p, vec3 v); // 点光源

void main() {
    vec3 result = calcDirLight(dirLight, norm, viewDir);
    for (int i = 0; i < NR_POINT_LIGHTS; i++)
        result += calcPointLight(pointLights[i], norm, FragPos, viewDir);
    FragColor = vec4(result, 1.0);
}
```

### 10.2 uniform 数组命名规则

```cpp
// C++ 端（QString 动态拼接）
shader.setVec3(QString("uPointLights[%1].position").arg(i), x, y, z);
shader.setVec3(QString("uPointLights[%1].diffuse" ).arg(i), r, g, b);
```

GLSL 端：

```glsl
uniform PointLight uPointLights[4];  // 4 = NR_POINT_LIGHTS
```

**重要**：`#define NR_POINT_LIGHTS 4` 在 GLSL 中即可，不需要从 C++ 传。

---

## 11. 光照贴图 (Lighting Maps)

对应课程：**lesson11_lighting_maps**

### 11.1 漫反射贴图 vs 镜面光贴图

| 贴图类型 | 替代什么 | 效果 |
|---|---|---|
| Diffuse Map  | `Material.diffuse` 颜色 | 木材/金属/布料等的固有颜色 |
| Specular Map | `Material.specular` 颜色 | 哪里能反光（白=有高光，黑=无高光） |

### 11.2 Fragment Shader 中的贴图采样

```glsl
struct Material {
    sampler2D diffuse;   // 替代 vec3 diffuse
    sampler2D specular;  // 替代 vec3 specular
    float shininess;
};

vec3 diffuseColor = texture(material.diffuse, TexCoord).rgb;
vec3 specularColor = texture(material.specular, TexCoord).rgb;

// 与之前一样算 Phong，只是颜色从贴图来
vec3 ambient = light.ambient * diffuseColor;
vec3 diffuse = light.diffuse * diff * diffuseColor;
vec3 specular = light.specular * spec * specularColor; // 被 specular map 调制
```

### 11.3 顶点属性扩展为 pos(3) + normal(3) + texcoord(2)

```cpp
// stride = 8 * sizeof(float)
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
```

---

## 12. 深度与混合

### 12.1 深度测试

```cpp
glEnable(GL_DEPTH_TEST);                      // 开启
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 每帧清两个缓冲
glDepthFunc(GL_LESS);                         // 默认值：近的覆盖远的
```

⚠️ 没有调 `format.setDepthBufferSize(24)` 或没 `glClear` 深度，会出现 **物体穿插错乱**。

### 12.2 混合（透明度）

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 经典透明
```

半透明物体需要按 **从远到近** 排序绘制。

### 12.3 面剔除

```cpp
glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);   // 剔除背面
glFrontFace(GL_CCW);   // 逆时针为正面（默认）
```

---

## 13. 资源管理与生命周期

| 阶段 | 该做什么 |
|---|---|
| `initializeGL`  | `glGen*` 生成、`glBufferData` 上传、`glTexImage2D` 上传、Shader 编译 |
| `resizeGL`      | `glViewport(0,0,w,h)`、必要时重建 FBO |
| `paintGL`       | `glClear` → `glUseProgram` → 绑 VAO/纹理 → `glDraw*` |
| 析构函数         | `makeCurrent()` → `glDelete*` → `doneCurrent()` |

```cpp
~OpenGLWidget()
{
    makeCurrent();
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_texture) glDeleteTextures(1, &m_texture);
    doneCurrent();
}
```

> 离开 `makeCurrent` 范围调用 `glDelete*` 是 **未定义行为**，务必包裹。

---

## 学习建议

1. **逐课跑通，不要跳**：第 02 课的 VBO/VAO 是后续一切的基础。
2. **多改少抄**：跑通后改顶点颜色、改三角形数量、改 uniform，立刻能看到反馈。
3. **配合 LearnOpenGL CN**：本项目章节顺序与之一致，文字解释可在网站上找到。
4. **遇到黑屏**：先看 [常见问题FAQ](常见问题FAQ.md#为什么我什么都看不到黑屏)。
5. **想看 GPU 内部**：用 RenderDoc 抓帧，可视化 VBO/纹理/uniform。
