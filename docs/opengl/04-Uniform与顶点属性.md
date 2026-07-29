# 04 · Uniform 与顶点属性

> 着色器需要的"外部数据"从哪来？顶点属性（每顶点不同）和 Uniform（全局共享）。

---

## 4.1 两种数据传输方式

```
                    顶点属性 (Attribute)        Uniform
                    ──────────────────        ────────
传给谁:              顶点着色器                  顶点 + 片段着色器
每什么变化:           每个顶点                   整个绘制调用
典型用途:             位置、法线、纹理坐标         MVP 矩阵、光源、材质
GLSL 声明:            layout(location=N) in     uniform
C++ 端配置:           glVertexAttribPointer     glUniform*
                      glEnableVertexAttribArray  (先 glGetUniformLocation)
```

---

## 4.2 Uniform —— 全局参数

### GLSL 端声明

```glsl
// 顶点着色器
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// 片段着色器
uniform vec3 uCameraPos;
uniform DirLight uLight;     // 自定义结构体也可以
uniform Material uMaterial;
```

### C++ 端设置

```c
// ① 激活着色器程序
glUseProgram(program);

// ② 获取 uniform 位置 (名字 → 整数 ID)
GLint loc = glGetUniformLocation(program, "uModel");

// ③ 设置值 (根据类型选不同函数)
glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
//                    ↑     ↑        ↑            ↑
//                位置    数量  是否转置     数据指针

// 其他常用:
glUniform3fv(loc, 1, glm::value_ptr(vec3Value));     // vec3
glUniform3f(loc, x, y, z);                            // 3 个 float
glUniform1f(loc, value);                               // 1 个 float
glUniform1i(loc, value);                               // 1 个 int
```

### 本项目 Shader 类的封装

```c
void Shader::setMat4(const std::string &name, const glm::mat4 &m) {
    glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setVec3(const std::string &name, const glm::vec3 &v) {
    glUniform3fv(loc(name), 1, glm::value_ptr(v));
}
void Shader::setFloat(const std::string &name, float v) {
    glUniform1f(loc(name), v);
}

// 内部缓存 uniform 位置:
GLint Shader::loc(const std::string &name) {
    return glGetUniformLocation(m_program, name.c_str());
}
```

---

## 4.3 顶点属性 —— 每个顶点的数据

### 完整流程回顾

```
   C++ 端                              GPU 端 (顶点着色器)
   ──────                              ────────────────────
                                     
   ① 准备数据                          
   float verts[] = {x,y,z, nx,ny,nz, ...};
                                     
   ② 上传到 VBO                        
   glBufferData(..., verts, ...);
                                     
   ③ 设置属性格式                      
   glVertexAttribPointer(0, 3, float,   →  layout(location=0) in vec3 aPos;
                          stride, offset);  aPos = verts[i*6+0], verts[i*6+1], verts[i*6+2]
                                     
   glVertexAttribPointer(1, 3, float,   →  layout(location=1) in vec3 aNormal;
                          stride, offset);  aNormal = verts[i*6+3], verts[i*6+4], verts[i*6+5]
                                     
   glEnableVertexAttribArray(0);        →  启用 location 0
   glEnableVertexAttribArray(1);        →  启用 location 1
                                     
   ④ 绘制                              
   glDrawArrays(GL_TRIANGLES, 0, N);   →  GPU 自动从 VBO 取数据，逐顶点调着色器
```

---

## 4.4 glVertexAttribPointer 参数详解

```c
glVertexAttribPointer(
    0,                  // ① location: 对应着色器的 layout(location=0)
    3,                  // ② size: 每个属性几个分量 (vec3 = 3, vec2 = 2)
    GL_FLOAT,           // ③ type: 分量类型
    GL_FALSE,           // ④ normalized: 是否需要归一化到 [0,1]
    6 * sizeof(float),  // ⑤ stride: 步长 = 连续两个顶点之间的字节数
    (void*)0            // ⑥ offset: 这个属性从顶点开头偏移多少字节
);
```

### 步长示例

```
VBO 内存布局:
| 顶点 0  | 顶点 0  | 顶点 1  | 顶点 1  | ...
| 位置    | 法线    | 位置    | 法线    |
| x y z  | nx ny nz| x y z  | nx ny nz|
  0       12        24       36        (字节偏移)

location=0 (位置):  offset=0,  stride=24
location=1 (法线):  offset=12, stride=24
```

---

## 4.5 Uniform 命名规范

本项目统一使用 `u` 前缀标记 Uniform：

```glsl
uniform mat4 uModel;        // u + PascalCase
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform vec3 uCameraPos;
```

好处：一行区分 `in` 和 `uniform`：

```glsl
in vec3 aPos;           // a = attribute (每顶点不同)
uniform mat4 uModel;    // u = uniform (全局共享)
out vec3 vFragPos;      // v = varying (从顶点传到片段)
```

---

## 4.6 常见错误

### 错误 1：拼写不一致

```c
// C++ 端:                           // GLSL 端:
glGetUniformLocation(prog, "uModel");  uniform mat4 uModel;  // ✅
glGetUniformLocation(prog, "umodel");  uniform mat4 uModel;  // ❌ 大小写不对
```

### 错误 2：忘记 glUseProgram

```c
// ❌ 错误: 没有先 use program
glUniformMatrix4fv(loc, 1, GL_FALSE, ...);

// ✅ 正确
glUseProgram(program);
glUniformMatrix4fv(loc, 1, GL_FALSE, ...);
```

### 错误 3：Uniform 没被使用

```glsl
uniform mat4 uUnusedMatrix;  // 声明了但着色器里没用到
```

→ 编译器会优化掉这个 uniform，`glGetUniformLocation` 返回 -1。不影响渲染，但浪费代码。

---

## 4.7 总结

```
两种数据通道:

  Attribute (顶点属性)
    每条数据不同 → 放 VBO → glVertexAttribPointer 说明格式
    GLSL: layout(location=N) in

  Uniform (全局参数)
    所有数据共享 → glUniform* 直接设值
    GLSL: uniform

  C++ 端给 Uniform 传值:
    ① glUseProgram
    ② glGetUniformLocation → 拿位置
    ③ glUniform* → 设值
```

---

**下一篇**: [05-矩阵与MVP坐标变换](./05-矩阵与MVP坐标变换.md) —— 3D 物体怎么投影到 2D 屏幕？
