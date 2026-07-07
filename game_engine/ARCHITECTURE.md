# Game Engine Editor — 架构文档

## 1. 项目概述

基于 **Flecs 4.1.6 ECS** + **Qt5 Widgets** + **OpenGL 3.3 Core** 的 3D 游戏引擎编辑器。

- 编辑器 UI：纯 Qt5 Widgets（DockWidget 布局）
- ECS 核心：Flecs C API（`flecs_no_addons.h`，编译为静态库）
- 渲染：前向 Phong 多光源管线
- 序列化：Qt JSON（场景保存/加载）
- 导出：生成独立 C++ 项目（main.cpp + CMakeLists.txt）

## 2. 技术栈

| 组件 | 技术 | 版本 | 备注 |
|------|------|------|------|
| ECS | Flecs | 4.1.6 (no_addons) | C API，编译为 STATIC 库 |
| UI | Qt5 Widgets | 5.14.2 | DockWidget + QTreeWidget + QFormLayout |
| 渲染 | OpenGL | 3.3 Core | QOpenGLWidget + QOpenGLFunctions_3_3_Core |
| 构建 | CMake | 3.15+ | C++20, MSVC 2022 |
| 字体 | 无 | — | 不依赖额外库 |

## 3. 目录结构

```
game_engine/
├── CMakeLists.txt                    # 构建系统（Flecs STATIC + Qt AUTOMOC + WIN32）
├── ARCHITECTURE.md                   # 本文档
├── API_REFERENCE.md                  # 组件/类 API 参考
├── TROUBLESHOOTING.md                # 常见问题
├── engine/                           # 引擎核心（header-only）
│   ├── engine.h                      # Engine 门面类
│   ├── ecs/
│   │   ├── components.h              # 7个组件定义
│   │   └── world.h                   # Flecs World 封装 + JSON 序列化
│   ├── render/
│   │   ├── shader.h                  # GLSL 着色器封装
│   │   ├── renderer.h               # 多光源 Phong 渲染器
│   │   ├── mesh_factory.h           # 程序化几何体 + OBJ 加载
│   │   └── shaders/                  # 4个GLSL着色器文件
│   └── resources/                    # (预留) 资源管理器
├── editor/                           # Qt5 编辑器面板
│   ├── main.cpp                      # 入口
│   ├── main_window.h                # 主窗口 (MenuBar + DockWidget)
│   ├── viewport.h                    # 3D 视口 (QOpenGLWidget)
│   ├── hierarchy_panel.h            # 层级树 (QTreeWidget)
│   ├── inspector_panel.h            # 属性检查器 (QFormLayout)
│   ├── asset_browser.h              # 资产浏览器 (QListWidget)
│   └── export_dialog.h              # 导出对话框 → C++ 源码
├── build/                            # CMake 构建输出
└── thirdparty/                       # 第三方库（用户手动引入）
```

## 4. 架构分层

```
┌────────────────────────────────────────────────────┐
│                  Editor Layer (Qt5)                 │
│  main_window.h  viewport.h  hierarchy_panel.h ...  │
├────────────────────────────────────────────────────┤
│              Engine Facade (engine.h)               │
│  createEntity()  setComponent()  cleanupGL()       │
├───────────────────────┬────────────────────────────┤
│   ECS (world.h)       │   Renderer (renderer.h)    │
│   Flecs C API         │   OpenGL 3.3 + Phong       │
│   ecs_world_t         │   Shader + MeshFactory     │
├───────────────────────┴────────────────────────────┤
│              第三方库: Flecs + Qt + OpenGL          │
└────────────────────────────────────────────────────┘
```

## 5. 数据流

```
[用户操作] → HierarchyPanel/InspectorPanel
    → Engine::setComponent()
    → ecs_set_ptr(world, entity, Component, &value)
    → Flecs 内部存储

[渲染循环] → Viewport::paintGL()
    → Renderer::beginFrame(world)  // 收集光源+摄像机
    → Renderer::renderEntities(world)  // 遍历 Mesh+Transform
    → glDrawElements/glDrawArrays
```

## 6. 组件系统

### 6.1 组件列表

| 组件 | 大小 | 用途 | 是否平凡可复制 |
|------|------|------|----------------|
| `TransformComponent` | 36B | 位置/旋转(欧拉)/缩放 | ✅ (QVector3D×3) |
| `MeshComponent` | 280B | VAO/VBO/EBO + 顶点计数 | ✅ (GLuint×3 + char[256]) |
| `MaterialComponent` | 92B | Phong 材质参数 + 纹理 | ✅ (QVector3D×3 + float + GLuint×2 + char[64]) |
| `LightComponent` | 80B | 光源类型/颜色/衰减 | ✅ (QVector3D×3 + float×5 + int) |
| `CameraComponent` | 60B | 位置/朝向/FOV | ✅ (QVector3D×3 + float×3 + bool) |
| `TagComponent` | 64B | 实体名称 | ✅ (char[64]) |
| `SkyboxComponent` | 4B | 天空盒纹理 ID | ✅ (GLuint) |

### 6.2 关键设计决策：`char[]` 替代 `QString`

**原因**：Flecs 使用 `memcpy` 复制组件数据。`QString` 内部使用堆分配和引用计数，
raw memcpy 会导致双重释放和悬空指针（错误：`qstringview.h:174 ASSERT: "len >= 0"`）。

**解决方案**：所有组件中需要字符串的字段使用固定大小 `char[N]` 数组替代 `QString`：

- `TagComponent::name` → `char[64]`
- `MaterialComponent::shaderName` → `char[64]`
- `MeshComponent::resourcePath` → `char[256]`

读写时使用 `qstrncpy()`（Qt 安全字符串复制）和 `QString::fromUtf8()` 转换。

### 6.3 组件 ID 注册

```cpp
// world.h 文件作用域（inline 避免多编译单元重复）
inline ecs_entity_t ecs_id(TransformComponent);
// ... (共6个)

// EngineWorld 构造函数
ECS_COMPONENT_DEFINE(m_world, TransformComponent);
// ... (共6个)
```

使用 **文件作用域 inline 声明** + **构造函数内 ECS_COMPONENT_DEFINE** 初始化，
因为 Qt AUTOMOC 会额外生成 `mocs_compilation_Debug.cpp` 编译单元。

## 7. 着色器系统

### 7.1 着色器文件

| 文件 | 用途 |
|------|------|
| `default.vert` | Phong 顶点着色器：pos(0) + normal(1) + texcoord(2) |
| `default.frag` | 多光源 Phong：支持方向光/点光源/聚光灯（最多8个） |
| `light_cube.vert` | 光源可视化标记（仅 position） |
| `light_cube.frag` | 纯白色输出 |

### 7.2 Shader 类

```cpp
class Shader : protected QOpenGLFunctions_3_3_Core {
    bool load(const QString &vPath, const QString &fPath);
    void use();
    void setInt/setFloat/setVec3/setMat3/setMat4(...);
};
```

- 支持移动语义（禁止拷贝，管理 GL 资源）
- 构造函数中不加载（延迟到 `load()` 调用时）
- 析构时 `glDeleteProgram`

### 7.3 SHADER_DIR 宏

CMake 注入绝对路径：

```cmake
target_compile_definitions(game_editor PRIVATE 
    SHADER_DIR="${ENGINE_DIR}/render/shaders/")
```

最终路径：`d:/code/.std/opengl/game_engine/engine/render/shaders/`

## 8. 编辑器 UI 架构

### 8.1 主窗口布局

```
┌─────────────────────────────────────────────────────┐
│ MenuBar: File | Edit | Export | Help                 │
├──────────┬────────────────────────┬─────────────────┤
│Hierarchy │     Viewport (中央)     │   Inspector     │
│Panel     │    QOpenGLWidget       │   Panel         │
│(左Dock)  │                        │   (右Dock)       │
├──────────┴────────────────────────┴─────────────────┤
│              Asset Browser (底部Dock)                 │
├─────────────────────────────────────────────────────┤
│ StatusBar: FPS | Entity Count                       │
└─────────────────────────────────────────────────────┘
```

### 8.2 面板通信

使用 Qt 信号/槽：

- HierarchyPanel::entitySelected → InspectorPanel::onEntitySelected
- HierarchyPanel::entitySelected → Viewport::onEntitySelected
- InspectorPanel::componentChanged → Viewport::update()

### 8.3 GL 初始化时序

```
MainWindow 构造 → Viewport 创建（无 GL 上下文）
  → widget.show()
  → QOpenGLWidget::initializeGL()
    → emit Viewport::glReady()
      → MainWindow 槽:
        → engine.init(SHADER_DIR)    // 加载着色器
        → createDefaultScene()       // 创建实体+网格
```

**关键**：引擎初始化和场景创建必须推迟到 `glReady` 信号之后，
因为 `Shader::load()` 和 `MeshFactory::create*()` 都依赖 GL 上下文。

## 9. 渲染管线

### 9.1 每帧流程

```
paintGL():
  1. glClear
  2. processKeyboard(dt)          // WASD 移动摄像机
  3. Renderer::beginFrame(world)
     → ecs_query_init(LightComponent) → 收集光源数据
     → ecs_query_init(CameraComponent) → 计算 view/proj 矩阵
  4. Renderer::renderEntities(world)
     → ecs_query_init(Transform, Mesh)
     → 按 Material::shaderName 排序（减少 shader 切换）
     → 对每个实体：
       - shader.use()
       - 设置 uView, uProjection, uCameraPos
       - 设置 uModel, uNormalMatrix
       - 设置 uMaterial.*, 纹理
       - 设置 uLights[] (applyLights)
       - glDrawElements/glDrawArrays
```

### 9.2 渲染优化

- **Shader 分组**：按 `Material::shaderName` 排序，减少 `glUseProgram` 切换
- **光源限制**：MAX_LIGHTS = 8
- **查询复用**：Flecs query 在每帧创建/销毁

## 10. 序列化系统

### 10.1 JSON 格式

```json
[
  {
    "name": "Ground",
    "id": 514,
    "transform": {"px":0,"py":-2,"pz":0, "rx":0,"ry":0,"rz":0, "sx":10,"sy":1,"sz":10},
    "material": {"ambient":..., "diffuse":..., "specular":..., "shininess":8}
  }
]
```

### 10.2 接口

```cpp
QString EngineWorld::exportToJson();           // 场景 → JSON 字符串
bool EngineWorld::importFromJson(const QString&); // JSON → 场景
```

使用 Qt QJsonDocument/QJsonObject/QJsonArray，零外部依赖。

## 11. 导出系统

ExportDialog 将当前场景导出为独立 C++ 项目：

```
exported_game/
├── main.cpp          # 运行时模板（QOpenGLWidget + 引擎渲染循环）
├── scene.json        # 场景数据
└── CMakeLists.txt    # 链接 game_engine 库的构建脚本
```

导出的 `main.cpp` 在 `initializeGL()` 中调用 `importFromJson()` 加载场景。

## 12. CMake 构建配置

### 12.1 关键设置

```cmake
project(GameEngine LANGUAGES C CXX)   # C 用于编译 flecs_no_addons.c
set(CMAKE_CXX_STANDARD 20)            # C++20（Flecs 指定初始化器）
set(CMAKE_AUTOMOC ON)                 # Qt MOC 自动生成
add_executable(game_editor WIN32 ...)  # GUI 应用（无终端窗口）
```

### 12.2 Flecs 编译

```cmake
add_library(flecs STATIC "${FLECS_DIR}/flecs_no_addons.c")
target_include_directories(flecs PUBLIC "${FLECS_DIR}")
```

Flecs 作为静态库编译（`flecs_no_addons.c`），避免链接时缺少符号。

### 12.3 Qt WinMain

```cmake
target_link_libraries(game_editor PRIVATE game_engine Qt5::WinMain)
```

`Qt5::WinMain` 提供 Windows GUI 入口点（`WinMain` → `main`）。

## 13. 已知问题与限制

| 问题 | 原因 | 状态 |
|------|------|------|
| MSB3073 后处理错误 | CMake 调用 `pwsh.exe` 失败 | ⚠️ 无害，不影响 exe |
| 选中实体线框高亮未实现 | `viewport.h` 中有 TODO | 🔧 待实现 |
| Assimp 模型加载未集成 | 缺少第三方库 | 🔧 待用户引入 |
| 材质贴图未实现 UI | asset_browser 仅有导入按钮 | 🔧 待实现 |
| 无物理引擎 | 计划使用 Bullet3 | 📋 后续 |

# Game Engine Editor — 架构文档

## 概述

基于 Flecs 4.1.6 ECS + Qt5 Widgets + OpenGL 3.3 的 3D 游戏引擎编辑器。
支持场景编辑、实体管理、实时渲染、JSON 序列化和 C++ 源码导出。

## 技术栈

| 组件 | 技术 | 版本 |
|------|------|------|
| ECS | Flecs | 4.1.6 (no_addons) |
| UI | Qt5 Widgets | 5.14.2 |
| 渲染 | OpenGL | 3.3 Core |
| 构建 | CMake | 3.15+ |
| 编译器 | MSVC | 2022 (C++20) |

## 目录结构

```
game_engine/
├── CMakeLists.txt                  # 构建系统
├── engine/                         # 引擎核心库 (header-only + Flecs .c)
│   ├── engine.h                    # Engine 门面类
│   ├── ecs/
│   │   ├── components.h            # 7 个组件定义 (Transform/Mesh/Material/Light/Camera/Tag/Skybox)
│   │   └── world.h                 # Flecs World 封装 + JSON 序列化
│   └── render/
│       ├── shader.h                # GLSL 着色器封装
│       ├── renderer.h              # 多光源 Phong 渲染器
│       ├── mesh_factory.h          # 程序化几何体 + OBJ 加载
│       └── shaders/                # GLSL 着色器文件
├── editor/                         # Qt5 编辑器
│   ├── main.cpp                    # 入口
│   ├── main_window.h              # 主窗口 (DockWidget 布局)
│   ├── viewport.h                  # 3D 视口 (WASD+鼠标摄像机)
│   ├── hierarchy_panel.h          # 层级树
│   ├── inspector_panel.h          # 属性检查器
│   ├── asset_browser.h            # 资产浏览器
│   └── export_dialog.h            # 导出 C++ 源码对话框
├── thirdparty/                     # 第三方库 (用户引入)
└── build/                          # 构建输出
```

## 架构设计

```
┌──────────────────────────────────────────────┐
│                  Editor UI                     │
│  ┌─────────┐ ┌──────────┐ ┌───────────────┐  │
│  │Hierarchy│ │ Viewport │ │  Inspector    │  │
│  │Panel    │ │ (OpenGL) │ │  Panel        │  │
│  └────┬────┘ └────┬─────┘ └───────┬───────┘  │
│       │           │               │           │
│  ┌────┴───────────┴───────────────┴────┐      │
│  │            Engine (门面)             │      │
│  └────────────────┬────────────────────┘      │
│                   │                            │
│  ┌────────────────┴────────────────────┐      │
│  │         EngineWorld (Flecs)         │      │
│  │  ┌──────────────────────────────┐   │      │
│  │  │ ecs_world_t (Flecs C API)    │   │      │
│  │  │ - 组件注册                    │   │      │
│  │  │ - 实体 CRUD                   │   │      │
│  │  │ - JSON 序列化                 │   │      │
│  │  └──────────────────────────────┘   │      │
│  └────────────────┬────────────────────┘      │
│                   │                            │
│  ┌────────────────┴────────────────────┐      │
│  │         Renderer (OpenGL)            │      │
│  │  - Phong 多光源渲染                  │      │
│  │  - 着色器管理                        │      │
│  │  - 网格工厂                          │      │
│  └─────────────────────────────────────┘      │
└──────────────────────────────────────────────┘
```

## 组件说明

| 组件 | 用途 | 关键字段 |
|------|------|----------|
| `TransformComponent` | 位置/旋转/缩放 | position, rotation(欧拉), scale |
| `MeshComponent` | 网格渲染 | vao, vbo, ebo, vertexCount, indexCount |
| `MaterialComponent` | 材质属性 | ambient, diffuse, specular, shininess, 纹理 |
| `LightComponent` | 光源 | type(方向/点/聚光), color, attenuation |
| `CameraComponent` | 摄像机 | position, front, up, fov, near/far |
| `TagComponent` | 实体名称 | name |

## 关键设计决策

1. **ECS_COMPONENT_DECLARE 使用 inline** — 避免 MOC 文件导致的重复符号 (LNK2005)
2. **ecs_set_ptr 替代 ecs_set** — MSVC C++20 不支持宏中的复合字面量
3. **setComponent 显式重载** — 模板参数 T 无法用于 `ecs_id(T)` 宏展开
4. **ecs_query_init + qd.terms** — 替代指定初始化器 `{.terms = {...}}`
5. **Flecs 编译为 STATIC 库** — 将 `flecs_no_addons.c` 编译链接
6. **CMAKE_AUTOMOC ON** — Qt Q_OBJECT 类自动生成 MOC 文件

## 构建

```bash
cd game_engine
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug --target game_editor -j 12
```

## 运行

```bash
.\build\Debug\game_editor.exe
```

**操作说明：**

- 右键拖拽：旋转视角
- WASD：移动摄像机
- 滚轮：缩放
- 菜单 Edit：添加 Cube/Light/Camera
- 层级树：选择/删除实体
- 属性面板：编辑组件属性
- File 菜单：保存/加载场景 (JSON)
- Export 菜单：导出为 C++ 独立项目
