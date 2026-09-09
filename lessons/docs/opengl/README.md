# OpenGL 完整教学

> 基于本项目引擎的实际代码，从零理解现代 OpenGL 渲染。

---

## 📚 阅读顺序（推荐）

| 序号 | 文档 | 核心内容 | 预计时间 |
| ------ | ------ | ---------- | ---------- |
| **01** | [渲染管线概述](./01-OpenGL渲染管线概述.md) | GPU 流水线各阶段、第一个三角形 | 15 分钟 |
| **02** | [VAO/VBO/EBO 与顶点数据](./02-VAO-VBO-EBO与顶点数据.md) | 数据上传 GPU、属性格式、索引绘制 | 20 分钟 |
| **03** | [GLSL 着色器基础](./03-GLSL着色器基础.md) | 顶点/片段着色器语法、编译链接流程 | 20 分钟 |
| **04** | [Uniform 与顶点属性](./04-Uniform与顶点属性.md) | C++ ↔ 着色器数据传输的两种方式 | 15 分钟 |
| **05** | [矩阵与 MVP 坐标变换](./05-矩阵与MVP坐标变换.md) | Model/View/Projection 矩阵、glm 用法 | 20 分钟 |
| **06** | [Blinn-Phong 光照模型](./06-Blinn-Phong光照模型.md) | 环境光+漫反射+镜面反射+半角向量 | 15 分钟 |
| **07** | [项目渲染流程走读](./07-实战-项目渲染流程走读.md) | 完整走读 paintGL 每一行代码 | 30 分钟 |

---

## 🎯 学完后你能做什么

- 理解本项目 `RenderWidget.cpp` 的每一行渲染代码
- 自己添加新的几何体（球、平面、模型导入）
- 实现新着色器效果（法线贴图、阴影、后处理）
- 看懂 LearnOpenGL 等教程的 C++/GLSL 代码

---

## 🔗 相关项目文件

- [Render/RenderWidget.cpp](../../../Render/RenderWidget.cpp) — 渲染核心（paintGL）
- [Render/Shader.h](../../../Render/Shader.h) — 着色器封装（编译/Uniform）
- [Render/MeshFactory.h](../../../Render/MeshFactory.h) — 几何体工厂 + 纹理创建
- [Render/ResourceManager.h](../../../Render/ResourceManager.h) — 资源句柄/缓存/延迟GPU
- [Components/TransformComponent.h](../../../Components/TransformComponent.h) — Model 矩阵（含 worldMatrix）
- [Components/MaterialComponent.h](../../../Components/MaterialComponent.h) — 材质数据
- [assets/Shaders/default.vert](../../../assets/Shaders/default.vert) — 顶点着色器
- [assets/Shaders/default.frag](../../../assets/Shaders/default.frag) — 片段着色器（多光源+阴影）

---

## 📖 延伸学习

- [LearnOpenGL-CN](https://learnopengl-cn.github.io/) — 最经典的中文 OpenGL 教程
- [OpenGL Wiki](https://www.khronos.org/opengl/wiki/) — 官方文档
- [GLM 手册](https://github.com/g-truc/glm/blob/master/manual.md) — 数学库参考
- [Qt OpenGL 文档](https://doc.qt.io/qt-6/qtopengl-index.html) — Qt 封装说明
