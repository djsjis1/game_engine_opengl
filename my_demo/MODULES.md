# My Demo Engine —— 学习文档

> 基于 Flecs ECS + Qt + OpenGL 的 3D 游戏引擎原型。
> 编辑栏模拟 Godot 的节点/组件浏览器。
> 本文档帮助你理解项目结构、学会添加功能、避免常见坑。

---

## 一、阅读顺序（推荐）

如果你第一次看这个项目，按这个顺序读代码：

| 顺序 | 文件 | 花几分钟 | 看懂什么 |
|------|------|---------|---------|
| 1 | `README.md`（本文档） | 10 | 全局架构 |
| 2 | `Engine.h` | 5 | ECS 引擎骨架，怎么注册组件和系统 |
| 3 | `System/System.h` | 2 | 所有系统的基类 |
| 4 | `System/RotationSystem.h` | 3 | 一个最简单的系统示例 |
| 5 | `MainWindow.cpp` | 10 | 游戏循环、键鼠输入、组装一切 |
| 6 | `RenderWidget.cpp` | 10 | 渲染管线（清屏→摄像机→遍历实体→绘制） |
| 7 | `Components/` 目录 | 5 | 了解有哪些组件 |
| 8 | `editor/EditorPanel.cpp` | 15 | 编辑栏 UI 怎么和 ECS 交互 |
| 9 | `CMakeLists.txt` | 5 | 构建配置和依赖管理 |

---

## 二、架构图

```
用户操作（编辑栏 / 键鼠）
        │
        ▼
┌─────────────────────────────────────────────────────┐
│                    MainWindow                        │
│                                                     │
│  ┌──────────┐  ┌────────────┐  ┌──────────────────┐ │
│  │EditorPanel│  │   Engine   │  │   RenderWidget   │ │
│  │  (Qt UI)  │  │ (ECS 核心) │  │  (OpenGL 视口)   │ │
│  │           │  │            │  │                  │ │
│  │ 实体列表   │  │ flecs::   │  │ paintGL() 每帧:  │ │
│  │ 组件列表   │──│  world ────│──│ 查摄像机→设MVP   │ │
│  │ 属性面板   │  │            │  │ 遍历实体→glDraw  │ │
│  │           │  │ systems[]  │  │                  │ │
│  └──────────┘  └────────────┘  └──────────────────┘ │
│                                                     │
│  eventFilter ── 全局键鼠捕获（WASD + 右键旋转）       │
│  QTimer(16ms) ── 游戏循环（60FPS）                   │
└─────────────────────────────────────────────────────┘
```

**数据总是从 ECS World 进出**：编辑栏写 World，System 读 World 改 World，RenderWidget 读 World 画。

---

## 三、摄像机系统（两种摄像机）

项目区分两种摄像机：

```
编辑器摄像机（isEditor = true）
  → 启动时自动创建，由 WASD + 右键拖拽控制
  → 你用它自由浏览场景、从各个角度看物体
  → CameraSystem 只更新这种摄像机

游戏摄像机（isEditor = false）
  → 你在编辑栏手动创建：新建实体 → 添加 CameraComponent
  → 放在场景中任意位置（改 Transform）
  → CameraSystem 不会碰它——保持静止
  → 可以放在立方体旁、门上、过道上
```

`active` 字段决定 `RenderWidget` 用哪个来渲染。如果没有 active 摄像机，使用硬编码默认视角（从 (0,3,8) 看向原点）。

---

## 四、目录结构

```
my_demo/
├── Components/              — 纯数据组件
│   ├── TransformComponent.h  位置/旋转/缩放 + getModelMatrix()
│   ├── MeshComponent.h       VAO/VBO/EBO 句柄 + isValid()
│   ├── MaterialComponent.h   ambient/diffuse/specular/shininess
│   ├── TagComponent.h        name[64]（实体名称）
│   ├── RotateBehavior.h      speedX/Y/Z（旋转速度）
│   └── CameraComponent.h     摄像机参数 + isEditor/active 标志
│
├── System/                  — 系统（逻辑层）
│   ├── System.h              系统基类（纯虚接口 update）
│   ├── RotationSystem.h      自动旋转（Transform + RotateBehavior）
│   ├── CameraSystem.h        编辑器摄像机控制（只操作 isEditor=true）
│   ├── CameraInput.h         键盘/鼠标输入状态结构体
│   ├── Shader.h              GLSL 编译/链接/Uniform
│   └── MeshFactory.h         生成几何体 VAO（当前只有立方体）
│
├── editor/                  — 编辑栏 UI
│   ├── EditorPanel.h/cpp     Godot 风格三面板
│
├── Shaders/                 — GLSL
│   ├── default.vert          MVP 变换 + 法线传参
│   └── default.frag          Blinn-Phong 方向光
│
├── Engine.h                 — 引擎骨架（World + System 管线）
├── RenderWidget.h/cpp       — OpenGL 渲染视口
├── MainWindow.h/cpp         — 主窗口（UI布局 + 游戏循环 + 输入）
├── main.cpp                 — 入口
├── CMakeLists.txt            — 构建配置
└── README.md                 — 本文档
```

---

## 五、完整数据流（从编辑到画面）

以 **"编辑栏改立方体位置"** 为例：

```
1. 用户拖动位置滑块
2. EditorPanel::onPropertyChanged()
   → 读 QDoubleSpinBox 值
   → comp_get_mut<TransformComponent>(m_world, e.raw_id())
   → 修改 tf->position
   → emit valueChanged()
3. MainWindow 收到信号
   → connect(m_editor, valueChanged, renderWidget, update())
4. paintGL() 执行
   → 清屏
   → 找 CameraComponent（取 viewMatrix、cameraPos）
   → 设全局 uniform（view、proj、灯光）
   → 遍历 (Transform + Mesh) 实体
     → tf.getModelMatrix() → uModel
     → MaterialComponent → uMaterial
     → glBindVertexArray → glDrawElements
```

以 **"旋转行为"** 为例：

```
1. 编辑栏给实体加 RotateBehavior，speedY = 90
2. 游戏循环每 16ms：
   Engine::runFrame(dt) → RotationSystem::update(world, dt)
   → world.each(Transform + RotateBehavior)
   → tf.rotation.y += 90 * 0.016 = 1.44°
3. 下一帧 paintGL 读出 rotation.y = 1.44°
   → 立方体看起来"自己在转"
```

---

## 六、每个文件详细说明

### Engine.h —— 引擎骨架

只有三个核心方法：

- `Engine()` 构造函数：注册所有组件类型
- `addSystem(sys)`：按顺序添加系统
- `runFrame(dt)`：按顺序执行所有系统

**扩展点**：新增组件在构造函数加 `m_world.component<NewComponent>()`。

---

### System/System.h —— 系统基类

所有系统继承它，实现 `update(world, dt)`。系统之间不直接通信——通过 ECS World 里的组件数据间接交互。

---

### Components/ —— 纯数据

每个组件是一个 `struct`，只包含数据字段和 const 辅助方法。

| 组件 | 关键字段 | 辅助方法 |
|------|---------|---------|
| TransformComponent | position, rotation, scale | getModelMatrix(), getNormalMatrix() |
| MeshComponent | vao, vbo, ebo, indexCount | isValid() |
| MaterialComponent | ambient, diffuse, specular, shininess | — |
| TagComponent | name[64] | — |
| RotateBehavior | speedX, speedY, speedZ | — |
| CameraComponent | position, front, yaw, pitch, fov | moveForward(), rotate(), updateView() |

CameraComponent 多了两个标志：

- `active`：RenderWidget 用它来决定用哪个摄像机渲染
- `isEditor`：CameraSystem 只更新 `isEditor=true` 的摄像机

---

### RotationSystem.h —— 最简系统示例

22 行代码。用 `world.each()` 遍历所有 `(Transform + RotateBehavior)` 实体，每帧把速度乘 dt 加到角度上。**看懂这个就等于看懂了怎么写系统**。

---

### CameraSystem.h —— 编辑器摄像机控制

和 RotationSystem 的最大区别：它依赖"外部数据"（CameraInput），通过 `setInput()` 注入，而不是只依赖组件数据。

只处理 `isEditor=true` 的摄像机，跳过场景中的游戏摄像机。

---

### CameraInput.h —— 输入状态结构体

MainWindow 的 `eventFilter` 把键盘/鼠标事件翻译成这个结构体。每帧 `resetFrame()` 清除鼠标增量（避免累积到下一帧）。

---

### RenderWidget —— 渲染管线

`paintGL()` 分三段：

1. **找摄像机**：从 ECS 查找 active 的 CameraComponent，取 viewMatrix
2. **设全局 uniform**：view、proj、灯光
3. **遍历实体**：`world.each(Transform + Mesh)` → 设模型矩阵、材质 → glDraw

---

### EditorPanel —— 编辑栏

三个面板：实体列表 → 组件列表 → 属性编辑器。任何修改都通过 `valueChanged()` 信号触发视口刷新。

**Mesh 组件的特殊处理**：创建 VAO 需要 OpenGL 上下文，所以通过 `m_meshCreator` 回调让 MainWindow 调用 `makeCurrent()` + `MeshFactory`。

---

### MainWindow —— 指挥官

- **构造函数**：创建 Engine → 注册 System → 组装 UI → 安装事件过滤器 → 启动游戏循环 → 创建默认摄像机
- **游戏循环**（每 16ms）：注入输入 → runFrame → 清除增量 → 刷新渲染
- **eventFilter**：全局捕获键鼠，`qApp->installEventFilter(this)` 确保不管焦点在哪里都能收到事件

---

## 七、代码模式速查

### 只读组件

```cpp
world.each([](const TransformComponent &tf) {
    glm::vec3 pos = tf.position;  // 不会改
});
```

### 修改组件

```cpp
world.each([](TransformComponent &tf) {
    tf.position.x += 1.0f;  // 非 const 引用，直接改
});
```

### 混合读写

```cpp
world.each([dt](TransformComponent &tf, const RotateBehavior &rb) {
    tf.rotation.y += rb.speedY * dt;  // 改 tf，只读 rb
});
```

### 可选组件

```cpp
// 不用 e.get<T>()（MSVC bug），用 C API 回退：
const auto *mat = static_cast<const MaterialComponent*>(
    ecs_get_id(m_world->c_ptr(), e.raw_id(),
               m_world->component<MaterialComponent>().raw_id()));
if (mat) { /* 有材质 */ } else { /* 默认值 */ }
```

### 外部数据注入

```cpp
class CameraSystem : public System {
    const CameraInput *m_input = nullptr;
    void setInput(const CameraInput *input) { m_input = input; }
    void update(world, dt) override {
        if (m_input->forward) { ... }
    }
};
// MainWindow 每帧：m_cameraSys->setInput(&m_input);
```

---

## 八、常见坑与解法

### 坑 1：Qt `emit` 宏污染

Qt 的 `#define emit` 破坏 Flecs 源码中的 emit 变量名。
**解法**：每个 `#include <flecs.h>` 前后加：

```cpp
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
```

### 坑 2：Flecs `e.get<T>()` 在 MSVC 崩溃

Flecs 4.1.6 的 `flecs::entity::get<T>()` 在 MSVC 有模板 bug。
**解法**：用 C API 绕过——`ecs_get_id(w->c_ptr(), id, w->component<T>().raw_id())`

### 坑 3：OpenGL 上下文

GPU 操作必须在活跃上下文中。
**解法**：`m_renderWidget->makeCurrent()` + 操作 + `doneCurrent()`

### 坑 4：编辑器焦点抢键盘

点击编辑栏输入框后，MainWindow 收不到键盘事件。
**解法**：`qApp->installEventFilter(this)` 全局捕获，返回 `false` 不拦截子控件。

### 坑 5：CameraComponent.projMatrix 是单位矩阵

CameraSystem 不更新 projMatrix（不知道窗口宽高比）。
**解法**：paintGL 里自己算 `proj = glm::perspective(...)`，只取 viewMatrix。

### 坑 6：Write 工具追加而非覆盖

Qoder 的 Write 工具有时会追加到文件末尾。
**解法**：发现文件有重复内容（两个 class 定义），删掉重写。

---

## 九、怎么添加新功能

### 加一个新组件（例如：生命值）

```
第 1 步：Components/HealthComponent.h
  struct HealthComponent { float hp = 100; float maxHp = 100; };

第 2 步：Engine.h 构造函数
  m_world.component<HealthComponent>();

第 3 步：（可选）写 System 操作它
第 4 步：（可选）EditorPanel 加 UI 编辑它
```

### 加一个新系统（例如：生命值恢复）

```
第 1 步：System/RegenSystem.h
  class RegenSystem : public System {
      void update(world, dt) override {
          world.each([dt](HealthComponent &hp) {
              if (hp.hp < hp.maxHp) hp.hp += 1.0f * dt;
          });
      }
  };

第 2 步：MainWindow.cpp 构造函数
  m_engine->addSystem(new RegenSystem);
```

---

## 十、构建与运行

```
1. VS Code 打开 my_demo/ 目录
2. Ctrl+Shift+P → CMake: Configure
3. F7 构建
4. F5 运行（或从命令行运行 build/my_demo/Debug/my_demo.exe）
```

**依赖**：Qt5（Widgets + OpenGL）、GLM 1.0.3、Flecs 4.1.6（C++ API，完整版）

---

## 附录：关键文件行数

| 文件 | 行数 | 说明 |
|------|------|------|
| Engine.h | 48 | 引擎骨架 |
| System.h | 46 | 系统基类 |
| RotationSystem.h | 38 | 旋转系统 |
| CameraSystem.h | 49 | 摄像机系统 |
| CameraInput.h | 33 | 输入状态 |
| CameraComponent.h | 77 | 摄像机组件 |
| RenderWidget.cpp | 143 | 渲染管线 |
| EditorPanel.cpp | 456 | 编辑栏 |
| MainWindow.cpp | 154 | 游戏循环+输入 |
| CMakeLists.txt | 129 | 构建配置 |
