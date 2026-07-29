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

**Mesh 组件的资源管理**（延迟化架构，与 GL 上下文彻底解耦）：

- `ResourceManager::acquireMesh` 是纯登记（任何时机可调），GPU 数据延迟到首帧渲染时创建
- 释放由 Engine 的 `MeshComponent OnRemove Observer` 自动触发（destruct/级联删除/移除组件全覆盖），归零后挂入待删队列，`paintGL` 每帧开头 flush
- 任何上层代码（命令/序列化/编辑器）都不需要 makeCurrent，也不需要"记得释放"

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

### 坑 3：OpenGL 上下文（已被架构升级化解）

GPU 操作必须在活跃上下文中。早期靠 `makeCurrent()` 包裹，到处都是样板代码。
**现行解法**：ResourceManager 延迟化——acquire/release 纯记账，GPU 创建/销毁
延迟到 paintGL（天然有上下文）。业务代码彻底不碰 GL。

### 坑 4：编辑器焦点抢键盘

点击编辑栏输入框后，MainWindow 收不到键盘事件。
**解法**：`qApp->installEventFilter(this)` 全局捕获，返回 `false` 不拦截子控件。

### 坑 5：CameraComponent.projMatrix 是单位矩阵

CameraSystem 不更新 projMatrix（不知道窗口宽高比）。
**解法**：paintGL 里自己算 `proj = glm::perspective(...)`，只取 viewMatrix。

### 坑 6：Write 工具追加而非覆盖

Qoder 的 Write 工具有时会追加到文件末尾。
**解法**：发现文件有重复内容（两个 class 定义），删掉重写。

### 坑 7：HFSM2 的 ContextT 默认拷贝 context（本项目最隐蔽的 bug）

`ContextT<EngineContext>` 是值语义：FSM 构造时拷贝一份 context。
Engine 构造 FSM 时回调还是空的，MainWindow 稍后注册的回调写进原件，
FSM 内部那份拷贝永远是空——**状态切换正常但 enter/exit 什么都不做**。
症状极具迷惑性：无崩溃无报错，只是 Play 模式的面板隐藏/快照/autoplay/
系统启停全部静默失效（存在数周未被发现）。
**解法**：`ContextT<EngineContext &>` 引用语义。
**通用教训**：把含状态对象交给第三方库时，先搞清它存引用还是拷贝；
排查手段是写最小隔离实验（本次靠三轮实验锁定：真实路径复现→
官方示例排除库本身→值/引用对照实锤）。

**孪生陷阱**：回调修好后析构路径也被激活——Play 状态直接关窗口时
`delete engine → FSM 析构 → exit → onExitPlay` 回调里的控件指针
可能已在销毁流程 → use-after-free。
**解法**：~MainWindow 先 `context() = EngineContext{}` 解绑再 delete。
**教训**：注册给库的回调捕获了 this，就必须在自己析构前解绑。

### 坑 8：Flecs 反射 member 两参数注册会猜错数组长度

`.member<T>("名字", 偏移)` 两参数形式下，flecs 用"到下个字段的偏移间隙"
推断 count。遇到未注册的运行时字段（如 modelMatrix）推断错 →
序列化出 `"y":[-2.25,3,0,45]` 这种错乱数据。
**解法**：必须三参数 `.member<T>("名字", 1, 偏移)` 显式 count=1
（现已由 reflect_gen 自动生成，不会再手写错）。

### 坑 9：flecs::entity 的 bool 不检查实体是否活着

`if (entity)` 只查 id!=0；对已删除实体调 `ecs_get_type/ecs_get_id` 直接崩溃。
症状：删除实体后点列表残留项 → 崩。
**解法**：任何外部持有的实体 ID 使用前先 `world.is_alive(id)`。

### 坑 10：QScrollArea::setWidget 会自动 delete 旧 widget

换新面板时旧面板被 Qt 默默销毁，复用的成员指针（如 m_emptyProp）变悬空
→use-after-free 崩溃。另外：控件自己的信号回调里不能重建面板（会销毁
自己），要 `QTimer::singleShot(0, ...)` 延迟到事件循环。
**教训**：Qt 父子/所有权转移 API 要阅文档确认谁负责 delete。

### 坑 11：静态单例不能持有 GL 资源对象

ResourceManager（静态单例）曾直接持有 MeshFactory（继承 QOpenGLFunctions）：
程序退出时静态析构晚于 GL 上下文销毁 → Qt 引用计数断言崩溃。
**解法**：堆分配 + 在上下文还活着时（cleanupGL）显式销毁。
**教训**：静态对象的析构时机不可控，不要让它们持有依赖外部生命周期的资源。

### 坑 12：WIN32 GUI 程序没有控制台，printf 诊断全部不可见

`add_executable(my_demo WIN32 ...)` → 无控制台，fprintf/print 看不见，
"控制台没输出"不能作为诊断依据。
**解法**：面向用户的诊断走 statusBar/UI；开发调试看 VS 调试输出或写日志文件。

### 坑 13：离散时间里用 == 判断时刻必然失效

动画事件帧若用 `time == 0.5` 触发：播放头每帧跳 16ms，几乎永远踩不中。
**解法**：区间跨越检测 `(prev, cur]`，loop 回绕拆两段（见 AnimClip::collectEvents）。
**教训**：一切"到达某时刻/某位置触发"的需求都要用跨越检测，不要用相等判断。

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
| CameraInput.h | 33 | 输入状态 |
| CameraComponent.h | 77 | 摄像机组件 |
| RenderWidget.cpp | 143 | 渲染管线 |
| EditorPanel.cpp | 456 | 编辑栏 |
| MainWindow.cpp | 154 | 游戏循环+输入 |
| CMakeLists.txt | 129 | 构建配置 |
