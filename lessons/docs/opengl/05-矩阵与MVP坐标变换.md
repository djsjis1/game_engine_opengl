# 05 · 矩阵与 MVP 坐标变换

> 3D 世界的物体怎么变成屏幕上的 2D 像素？答案是 MVP 三个矩阵。

---

## 5.1 为什么需要坐标变换？

```
一个立方体的顶点坐标从诞生到屏幕，经历 4 个空间:

  模型空间 (Local Space)      立方体自身坐标，以自己中心为原点
      ↓  × Model 矩阵
  世界空间 (World Space)      放在场景中的坐标
      ↓  × View 矩阵
  观察空间 (View Space)       以摄像机为原点的坐标
      ↓  × Projection 矩阵
  裁剪空间 (Clip Space)       GPU 用的归一化坐标 (-1 到 1)
      ↓  GPU 自动
  屏幕空间 (Screen Space)     实际的像素位置
```

### 完整公式

```
gl_Position = Projection × View × Model × vec4(localPos, 1.0)
              ─────────────────────────────────────────
                          MVP 矩阵
```

---

## 5.2 Model 矩阵 —— 从模型空间到世界空间

### 本项目的实现 (TransformComponent)

```cpp
struct TransformComponent {
    glm::vec3 position{0, 0, 0};   // 平移
    glm::vec3 rotation{0, 0, 0};   // 旋转 (欧拉角，度)
    glm::vec3 scale{1, 1, 1};      // 缩放

    glm::mat4 getModelMatrix() const {
        glm::mat4 m(1.0f);          // 单位矩阵
        
        // ① 平移: 把物体放到世界中的某个位置
        m = glm::translate(m, position);
        
        // ② 旋转: 绕 X、Y、Z 轴旋转 (注意顺序!)
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1,0,0));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0,1,0));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0,0,1));
        
        // ③ 缩放: 放大或缩小
        m = glm::scale(m, scale);
        
        return m;
    }
};
```

### 效果示例

```
原始立方体 (模型空间):      应用 Model 矩阵后 (世界空间):
  边长 1×1×1                 放到世界坐标 (2, 0, 5)
  中心在原点                 绕 Y 轴转了 45°
                             放大 2 倍
```

---

## 5.3 View 矩阵 —— 从世界空间到观察空间

### 通俗理解

```
View 矩阵 = 移动整个"世界"，而不是移动"摄像机"

实际上:
  摄像机在世界坐标 (0, 3, 8)，看向 (0, 0, 0)
  → View 矩阵把整个世界反向平移 (0, -3, -8)
  → 结果一样：摄像机在原点，世界在它面前
```

### 构建 View 矩阵

```cpp
glm::vec3 cameraPos   = glm::vec3(0, 3, 8);  // 摄像机位置
glm::vec3 cameraTarget = glm::vec3(0, 0, 0);  // 看向哪里
glm::vec3 up           = glm::vec3(0, 1, 0);  // 哪个方向是"上"

glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, up);
```

### 本项目的摄像机

```cpp
// 编辑器摄像机 (EditorCamera)
viewMatrix = glm::lookAt(position, position + front, glm::vec3(0,1,0));
//                        ↑          ↑                  ↑
//                     摄像机位置  摄像机看的方向        世界"上"方向

// 场景摄像机 (CameraComponent)
viewMatrix = glm::lookAt(position, position + front, up);
```

---

## 5.4 Projection 矩阵 —— 从观察空间到裁剪空间

### 两种投影

```
透视投影 (Perspective):              正交投影 (Orthographic):
  近大远小 (真实世界效果)              远近一样大 (工程图、UI)
  
      ┌─┐                               ┌─┐
     ╱   ╲                              │ │
    ╱     ╲                             │ │
   ╱ 物体   ╲                           │ │
  ╱         ╲                           └─┘
 相机       远处                         相机     远处
```

### 构建透视投影

```cpp
float fov   = 45.0f;                      // 视野角度 (度)
float aspect = width / (float)height;     // 宽高比
float near   = 0.1f;                      // 近平面
float far    = 100.0f;                    // 远平面

glm::mat4 proj = glm::perspective(
    glm::radians(fov), aspect, near, far);
```

### fov 的影响

```
fov = 45°               fov = 90°               fov = 120°
┌─────────────┐         ┌───────────────────┐    ┌═══════════════════┐
│   看得窄      │         │      看得宽        │    │     鱼眼效果      │
│   物体大      │         │      物体小        │    │     严重变形      │
└─────────────┘         └───────────────────┘    └═══════════════════┘
```

---

## 5.5 法线矩阵 (Normal Matrix)

### 问题：法线不能直接用 Model 矩阵变换

```
原始法线: n = (0, 0, 1)

如果 Model 矩阵有非均匀缩放 (如 scale.x=2, scale.y=1, scale.z=1):

  Model × n = 不合适！法线方向会被扭曲

正确做法: 用法线矩阵 = Model 矩阵的逆转置
  NormalMatrix = transpose(inverse(Model))
  取左上 3×3 部分 = mat3(NormalMatrix)
```

### 本项目的实现

```cpp
glm::mat3 getNormalMatrix() const {
    return glm::mat3(glm::transpose(glm::inverse(getModelMatrix())));
}
```

---

## 5.6 完整渲染一帧的矩阵设置

```cpp
// RenderWidget::paintGL() 中的实际代码:

// ① 获取摄像机矩阵
glm::mat4 view = m_editorCam->viewMatrix;  // 从 EditorCamera
glm::mat4 proj = m_editorCam->projMatrix;

// ② 设置全局 Uniform
m_shader.use();
m_shader.setMat4("uView", view);
m_shader.setMat4("uProjection", proj);
m_shader.setVec3("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

// ③ 遍历每个实体，设置其 Model 矩阵
m_world->each([this](flecs::entity e, TransformComponent &tf, const MeshComponent &ms) {
    // 每个实体的 Model 矩阵不同!
    m_shader.setMat4("uModel", tf.getModelMatrix());
    m_shader.setMat3("uNormalMatrix", tf.getNormalMatrix());
    
    // 绘制
    glBindVertexArray(ms.vao);
    glDrawElements(ms.primitiveType, ms.indexCount, GL_UNSIGNED_INT, 0);
});
```

### 为什么 View/Proj 只设置一次，Model 每实体都要设？

```
View/Proj → 摄像机不变 → 所有物体共享同一个
Model    → 每个物体位置不同 → 每个物体单独设置
```

---

## 5.7 glm 常用函数速查

| 函数 | 作用 |
|------|------|
| `glm::translate(mat, vec3)` | 平移 |
| `glm::rotate(mat, angle, axis)` | 旋转（角度弧度制，用 `glm::radians` 转换） |
| `glm::scale(mat, vec3)` | 缩放 |
| `glm::perspective(fov, aspect, near, far)` | 透视投影矩阵 |
| `glm::lookAt(pos, target, up)` | 视图矩阵 |
| `glm::radians(degrees)` | 度 → 弧度 |
| `glm::normalize(vec)` | 归一化向量 |
| `glm::cross(a, b)` | 叉积 |
| `glm::transpose(mat)` | 转置 |
| `glm::inverse(mat)` | 逆矩阵 |
| `glm::value_ptr(mat)` | 获取 float* 指针（传给 OpenGL） |

---

## 5.8 旋转顺序陷阱

```cpp
// 本项目的旋转顺序: X → Y → Z
m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1,0,0));  // 先绕 X
m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0,1,0));  // 再绕 Y
m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0,0,1));  // 再绕 Z

// ⚠ 不同的顺序 = 不同的结果!
// 先绕 X 再绕 Y ≠ 先绕 Y 再绕 X
```

---

## 5.9 总结

```
顶点从出生到屏幕，经历 5 次空间变换:

  模型空间 ──Model──→ 世界空间 ──View──→ 观察空间 ──Proj──→ 裁剪空间 ──GPU──→ 屏幕

  对应的矩阵:
    Model     = translate × rotate × scale      (每实体不同)
    View      = lookAt(camPos, target, up)       (全局 1 个)
    Projection = perspective(fov, aspect, n, f)  (全局 1 个)

  顶点着色器中:
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
```

---

**下一篇**: [06-Blinn-Phong光照模型](./06-Blinn-Phong光照模型.md) —— 光打上去怎么算颜色？
