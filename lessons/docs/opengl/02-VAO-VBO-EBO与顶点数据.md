# 02 · VAO / VBO / EBO 与顶点数据

> GPU 不会直接读 CPU 内存。数据必须"上传"到 GPU 显存。VAO/VBO/EBO 就是这套上传机制。

---

## 2.1 为什么需要 VBO？

```
问题：CPU 和 GPU 的物理内存在不同的地方

  CPU 内存 (RAM)           GPU 显存 (VRAM)
  ┌──────────────┐         ┌──────────────┐
  │ float verts[] │  ─?─→  │ ???          │
  │ = {           │         │              │
  │   -0.5,-0.5,  │         │  需要拷贝！   │
  │    0.5,-0.5,  │         │              │
  │    0.0, 0.5   │         │              │
  │ };            │         │              │
  └──────────────┘         └──────────────┘

解答：VBO 就是 GPU 显存里的一块缓冲区
      把 CPU 数据拷贝过去，GPU 就能直接读了
```

---

## 2.2 三个概念的关系

```
            VAO (顶点数组对象)
           ┌─────────────────────────────┐
           │  一份"配置清单"，记录:        │
           │                             │
           │  → VBO 在哪？               │
           │  → 每个顶点怎么解读？         │
           │    · 位置 3 个 float         │
           │    · 法线 3 个 float         │
           │    · 纹理坐标 2 个 float      │
           │                             │
           │  → EBO 在哪？(可选)          │
           │                             │
           │ 绑定 VAO 后，glDraw* 自动    │
           │ 用这份配置来取数据            │
           └──────┬──────────┬───────────┘
                  │          │
         ┌────────▼──┐  ┌───▼──────────┐
         │   VBO     │  │    EBO       │
         │ 顶点数据   │  │  索引数据     │
         ├───────────┤  ├──────────────┤
         │ v0: x,y,z │  │ 0, 1, 2,     │
         │ v1: x,y,z │  │ 0, 2, 3,     │
         │ v2: x,y,z │  │ 2, 1, 4,     │
         │ v3: x,y,z │  │ ...          │
         │ ...       │  └──────────────┘
         └───────────┘
```

### 类比记忆

```
VAO = 一份"录取通知书"，上面写着:
      "去 VBO 地址 XXX 处取数据,
       前 3 个 float 是位置,
       后 3 个 float 是法线,
       索引去 EBO 取"

VBO = 一个"大数组"，里面塞满了 float，按顺序排好

EBO = 一个"索引本"，告诉 GPU "别重复存顶点，
       用索引引用已有顶点节省显存"
```

---

## 2.3 手把手：创建 VAO + VBO

```c
// ① 准备顶点数据 (在 CPU 内存中)
float vertices[] = {
    // 位置(x,y,z)    法线(nx,ny,nz)
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // 顶点 0
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // 顶点 1
     0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // 顶点 2
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // 顶点 3
};

// ② 生成 GPU 资源 (只是拿到"名字"，还没有数据)
GLuint vao, vbo;
glGenVertexArrays(1, &vao);  // 生成 1 个 VAO，返回 ID
glGenBuffers(1, &vbo);       // 生成 1 个 VBO，返回 ID

// ③ 绑定 VAO (后续所有操作都记录在这个 VAO 下)
glBindVertexArray(vao);

// ④ 绑定 VBO，上传数据
glBindBuffer(GL_ARRAY_BUFFER, vbo);  // "我要操作这个 VBO"
glBufferData(GL_ARRAY_BUFFER,        // 把 CPU 数据拷贝到 GPU
             sizeof(vertices),       // 数据大小
             vertices,               // 数据指针
             GL_STATIC_DRAW);        // 使用提示 (数据不变)

// ⑤ 告诉 GPU 怎么解读这份数据 (顶点属性)
//    位置 (location = 0): 每个顶点开头 3 个 float
glVertexAttribPointer(0,                 // location = 0
                      3,                 // 3 个分量 (x, y, z)
                      GL_FLOAT,          // 每个分量是 float
                      GL_FALSE,          // 不需要归一化
                      6 * sizeof(float), // 步长: 每个顶点 6 个 float
                      (void*)0);         // 偏移: 从第 0 个字节开始
glEnableVertexAttribArray(0);            // 启用 location 0

//    法线 (location = 1): 每个顶点后 3 个 float
glVertexAttribPointer(1,                  // location = 1
                      3,                  // 3 个分量
                      GL_FLOAT,
                      GL_FALSE,
                      6 * sizeof(float),  // 步长同上
                      (void*)(3 * sizeof(float))); // 偏移: 跳过前 3 个 float
glEnableVertexAttribArray(1);

// ⑥ 解绑 (好习惯)
glBindVertexArray(0);
```

### 步长和偏移图解

```
VBO 里的数据 (byte 视图):

|←── 顶点 0 ──→|←── 顶点 1 ──→|←── 顶点 2 ──→|
|  x |  y |  z | nx | ny | nz |  x |  y |  z |...
  0    4    8    12   16   20   24   28   32   (字节偏移)

location=0 (位置): 偏移=0,  步长=24, 数量=3
location=1 (法线): 偏移=12, 步长=24, 数量=3
```

---

## 2.4 EBO —— 复用顶点，节省显存

### 不用 EBO（浪费）

```
画一个矩形需要 2 个三角形 = 6 个顶点

  v0──v1      三角形 1: v0, v1, v2
  │ \  │      三角形 2: v0, v2, v3
  │  \ │      
  v3──v2      共 6 个顶点，v0 和 v2 存了两次
```

### 用 EBO（高效）

```c
float vertices[] = {
    //  位置            法线
    -0.5f, -0.5f, 0.0f,  0,0,1,  // v0
     0.5f, -0.5f, 0.0f,  0,0,1,  // v1
     0.5f,  0.5f, 0.0f,  0,0,1,  // v2
    -0.5f,  0.5f, 0.0f,  0,0,1,  // v3
};
// 4 个顶点就够了！

GLuint indices[] = {
    0, 1, 2,  // 三角形 1: v0→v1→v2
    0, 2, 3   // 三角形 2: v0→v2→v3
};
// 6 个索引，指向 4 个顶点

// 创建 EBO
GLuint ebo;
glGenBuffers(1, &ebo);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

// 绘制时用 glDrawElements (不是 glDrawArrays)
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//                             ↑        ↑
//                         6 个索引  索引类型
```

**效果**：4 顶点 + 6 索引 = 10 个数字，而不是 6 顶点 = 36 个数字。立方体节省更明显（24 顶点 vs 36 顶点）。

---

## 2.5 本项目实际代码：MeshFactory::createCube

```c
void MeshFactory::createCube(MeshComponent &outMesh) {
    // ① 准备顶点和索引
    std::vector<Vertex> vertices;  // 24 个顶点
    std::vector<GLuint> indices;   // 36 个索引
    // ... 填充数据 ...

    // ② 创建 GPU 资源
    glGenVertexArrays(1, &outMesh.vao);
    glGenBuffers(1, &outMesh.vbo);
    glGenBuffers(1, &outMesh.ebo);

    glBindVertexArray(outMesh.vao);

    // ③ 上传 VBO
    glBindBuffer(GL_ARRAY_BUFFER, outMesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    // ④ 上传 EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outMesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);

    // ⑤ 设置顶点属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*)0);        // 位置
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*)(3*sizeof(float))); // 法线
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    outMesh.vertexCount = vertices.size();
    outMesh.indexCount  = indices.size();
    outMesh.hasIndices  = true;
}
```

### 项目中的 MeshComponent

```c
struct MeshComponent {
    GLuint vao = 0;             // VAO 句柄
    GLuint vbo = 0;             // VBO 句柄
    GLuint ebo = 0;             // EBO 句柄
    GLsizei vertexCount = 0;    // 顶点数量
    GLsizei indexCount = 0;     // 索引数量
    GLenum primitiveType = GL_TRIANGLES;
    bool hasIndices = false;

    bool isValid() const { return vao != 0; }
};
```

**每个实体持有一个 MeshComponent**，绘制时：

```c
glBindVertexArray(mesh.vao);
if (mesh.hasIndices)
    glDrawElements(mesh.primitiveType, mesh.indexCount, GL_UNSIGNED_INT, 0);
else
    glDrawArrays(mesh.primitiveType, 0, mesh.vertexCount);
```

---

## 2.6 使用提示 (GL_STATIC_DRAW 等)

| 提示 | 含义 | 适用场景 |
|------|------|----------|
| `GL_STATIC_DRAW` | 数据不变，多次使用 | 静态几何体（地面、建筑） |
| `GL_DYNAMIC_DRAW` | 数据经常变，多次使用 | 变形动画 |
| `GL_STREAM_DRAW` | 数据每帧都变，只用一次 | 粒子系统 |

---

## 2.7 总结

```
VAO/VBO/EBO 三部曲:

  ① glGen*    → 生成 GPU 资源（拿"名字"）
  ② glBind*   → 绑定（"我要操作这个"）
  ③ glBufferData → 上传数据（CPU → GPU）
  ④ glVertexAttribPointer → 告诉 GPU 怎么解读数据
  ⑤ glEnableVertexAttribArray → 启用属性
  ⑥ glBindVertexArray(0) → 解绑

  绘制时: glBindVertexArray(vao) → glDraw*
```

---

**下一篇**: [03-GLSL着色器基础](./03-GLSL着色器基础.md) —— 怎么写 GPU 上跑的着色器程序？
