# 06 · Blinn-Phong 光照模型

> 一束光打在一个像素上，最终颜色怎么算？三个分量加起来。

---

## 6.1 光照的三个分量

```
材质最终颜色 = 环境光 + 漫反射 + 镜面反射

  Ambient   (环境光)       Diffuse   (漫反射)       Specular  (镜面反射)
  ┌─────────────┐        ┌─────────────┐        ┌─────────────┐
  │ 到处都是      │        │ 面朝光源越正  │        │ 高光亮斑      │
  │ 一样的亮度    │        │ 越亮          │        │ 视角越近反射  │
  │ 防止全黑      │        │ 体现立体感    │        │ 方向越亮      │
  │             │        │             │        │             │
  │ ████████████│        │ ░░░░████░░░░│        │ ░░░░▓▓░░░░░│
  │ ████████████│        │ ░░████████░░│        │ ░░████▓▓░░░│
  └─────────────┘        └─────────────┘        └─────────────┘
         +                       +                       =
  ┌─────────────────────────────────────────────────────────┐
  │                    ░░████▓▓░░░░                          │
  │                 ░░████████▓▓░░░                          │
  │                  最终效果 = 三合一                         │
  └─────────────────────────────────────────────────────────┘
```

---

## 6.2 环境光 (Ambient)

### 最简单的分量

```
环境光 = 光源环境色 × 材质环境反射率

所有地方都一样亮，不依赖方向。
作用：防止背光面完全黑（现实中物体暗面也有微弱光线）。
```

```glsl
vec3 ambient = uLight.ambient * uMaterial.ambient;
//             ↑ 光源的环境色      ↑ 材质的环境光反射率
//             如 (0.2,0.2,0.2)   如 (0.2,0.2,0.2)
//             → 结果 = (0.04, 0.04, 0.04)  很暗的灰色
```

---

## 6.3 漫反射 (Diffuse) —— Lambert 模型

### 核心公式

```
漫反射强度 = max(法线·光线方向, 0)

             光源
              ☀
               \
                \  ← 光线方向 L
                 \
    ────────────── →法线 N
    ║███████████████████████║
    
    N 正对 L → N·L = 1 → 最亮
    N 侧对 L → N·L < 1 → 较暗
    N 背对 L → N·L < 0 → 全部变 0 (max 的作用)
```

### GLSL 代码

```glsl
vec3 norm = normalize(vNormal);                // ① 法线归一化
vec3 lightDir = normalize(-uLight.direction);  // ② 指向光源的方向
float diff = max(dot(norm, lightDir), 0.0);    // ③ 夹角余弦 (≥ 0)
vec3 diffuse = uLight.diffuse * diff * uMaterial.diffuse;
//             ↑光源漫反射色    ↑亮度系数    ↑材质漫反射率
```

### 示例

```
光源方向从上往下 (-0.3, -0.7, -0.5)，法线朝上 (0, 1, 0):

  N = (0, 1, 0)
  L = normalize(0.3, 0.7, 0.5) = (0.33, 0.77, 0.55)
  N·L = 0×0.33 + 1×0.77 + 0×0.55 = 0.77
  → 漫反射强度 = 0.77 (较亮)
```

---

## 6.4 镜面反射 (Specular) —— Blinn-Phong 模型

### Phong vs Blinn-Phong

```
  Phong 模型                    Blinn-Phong 模型 (本项目)
  ──────────                    ──────────────────────
  计算反射向量 R                 计算半角向量 H = normalize(L + V)
  再算 R·V                      再算 N·H
  
  更物理正确                    计算更快、效果差不多
```

### 核心公式

```
半角向量 H = normalize(L + V)
镜面强度 = max(N·H, 0)^shininess

    光源 ☀        摄像机 👁
         \       ╱
          \     ╱
           \   ╱
            \ ╱
    ──────── ● ─────  ← 法线 N
             H  (半角向量)
             
  H 越接近 N → N·H 越大 → 高光越亮
  shininess 越大 → 高光越集中 (越锐利)
```

### GLSL 代码

```glsl
vec3 viewDir = normalize(uCameraPos - vFragPos);     // 指向摄像机的方向
vec3 halfwayDir = normalize(lightDir + viewDir);     // 半角向量 H
float spec = pow(max(dot(norm, halfwayDir), 0.0),    // (N·H)^shininess
                 uMaterial.shininess);
vec3 specular = uLight.specular * spec * uMaterial.specular;
```

### shininess 的效果

```
shininess = 2          shininess = 32          shininess = 256
─────────────────      ──────────────────      ──────────────────
     ░░▓▓▓▓░░░             ░░░░▓▓░░░             ░░░░░▓░░░░
   ░░▓▓▓▓▓▓▓▓░░          ░░░▓▓▓▓▓▓░░░          ░░░░▓▓▓░░░░
  ░░▓▓▓▓▓▓▓▓▓▓░         ░░▓▓▓▓▓▓▓▓▓▓░         ░░░▓▓▓▓▓░░░
     模糊大光斑              中等锐度               锐利小光点
```

---

## 6.5 完整公式（本项目 default.frag）

```glsl
void main() {
    // ① 环境光
    vec3 ambient = uLight.ambient * uMaterial.ambient;

    // ② 漫反射
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(-uLight.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLight.diffuse * diff * uMaterial.diffuse;

    // ③ 镜面反射
    vec3 viewDir = normalize(uCameraPos - vFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess);
    vec3 specular = uLight.specular * spec * uMaterial.specular;

    // ④ 最终颜色
    vec3 result = ambient + diffuse + specular;
    fragColor = vec4(result, 1.0);
}
```

---

## 6.6 本项目的 Uniform 设置

```cpp
// RenderWidget::paintGL() 中:

m_shader.setVec3("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

// 方向光 (模拟太阳光)
m_shader.setVec3("uLight.direction", -0.3f, -0.7f, -0.5f);  // 从上往下
m_shader.setVec3("uLight.ambient",   0.2f,  0.2f,  0.2f);   // 暗灰
m_shader.setVec3("uLight.diffuse",   0.8f,  0.8f,  0.8f);   // 亮灰
m_shader.setVec3("uLight.specular",  1.0f,  1.0f,  1.0f);   // 白色

// 材质 (从 ECS 组件读取)
m_shader.setVec3("uMaterial.ambient",   mat->ambient);
m_shader.setVec3("uMaterial.diffuse",   mat->diffuse);
m_shader.setVec3("uMaterial.specular",  mat->specular);
m_shader.setFloat("uMaterial.shininess", mat->shininess);
```

---

## 6.7 方向光 vs 点光源

本项目只用方向光（最简单的光源）：

```
方向光 (Directional Light):         点光源 (Point Light):
  所有光线平行 (像太阳)               光线从一个点辐射出去
  只需一个方向向量                    需要位置 + 衰减公式
  uniform vec3 direction;            uniform vec3 position;
                                    衰减 = 1/(常数 + 线性×d + 二次×d²)
```

---

## 6.8 总结

```
光照 = 环境光 + 漫反射 + 镜面反射

  环境光      = light.ambient  × material.ambient
  漫反射      = light.diffuse  × max(N·L, 0) × material.diffuse
  镜面反射    = light.specular × max(N·H, 0)^shininess × material.specular
              (H = 半角向量 = normalize(L + V))

  最终颜色    = 环境光 + 漫反射 + 镜面反射
```

---

**下一篇**: [07-实战-项目渲染流程走读](./07-实战-项目渲染流程走读.md) —— 从 MainWindow 到屏幕的完整渲染过程
