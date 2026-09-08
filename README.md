# 3D 游戏引擎 × OpenGL 学习项目

基于 **Flecs ECS + Qt + OpenGL + Lua + Jolt Physics** 的 3D 游戏引擎原型，附带 21 节渐进式 OpenGL 学习课程。

> 引擎对标 Godot（编辑器三栏 UI + 内置脚本编辑器 + 导出游戏独立 exe）。
> 能用三种方式做游戏：编辑器摆场景 / 脚本挂行为 / 纯代码 bootstrap。

---

## 一、项目结构

```
opengl/
├── CMakeLists.txt              # 引擎构建脚本（主目标：my_demo / my_game）
├── main.cpp                    # 编辑器入口
├── MainWindow.h / .cpp         # 编辑器主窗口（三栏 UI）
│
├── Core/                       # 引擎基础设施（无 UI 依赖）
│   ├── Engine.h                #   World + System 管线 + 状态机
│   ├── Input.h / Time.h        #   全局输入/时钟单例
│   ├── SignalQueue.h           #   多线程信号队列（ADR-002）
│   └── ...                     #   SaveGame/SceneRouter/NavGrid/Easing/...
│
├── Components/                 # ECS 纯数据组件（REFLECT 宏自动反射）
├── Systems/                    # 逻辑系统（Camera/Rotation/Physics/Audio/Tween/...）
├── Scripting/                  # Lua 脚本层（API 按域拆分 + 热重载）
├── Render/                     # 渲染模块（多 Pass 管线：阴影→天空→后处理）
├── editor/                     # 编辑器 UI（场景树/Inspector/脚本编辑器）
├── Game/                       # 游戏运行时（导出后只有视口窗口）
│
├── assets/                     # 资源（Shaders/scenes/scripts/anims/sprites/prefabs）
├── Generated/                  # 自动生成（ComponentReflection.inl）
├── tools/                      # reflect_gen 反射代码生成器
├── tests/                      # 测试（unit/ + integration/ + engine/）
│
├── lessons/                    # OpenGL 渐进式学习课程（21 节，独立可运行）
│   ├── CMakeLists.txt
│   ├── lesson01_hello_window/ ... lesson21_deferred_shading/
│   └── docs/                   #   课程配套文档（flecs/hfsm2/lua/opengl）
│
├── docs/                       # 引擎文档
│   ├── 模块文档.md             #   架构总览 + 模块职责
│   ├── 脚本API手册.md          #   全部 Lua API 手册
│   ├── 排障手册.md             #   实战坑 + 排障方法论
│   └── adr/                    #   架构决策记录
│
├── thirdparty/                 # 第三方库（源码内嵌，锁定版本）
│   ├── flecs-4.1.6/            #   ECS 框架
│   ├── glm-1.0.3/              #   数学库
│   ├── lua-5.4.7/              #   脚本引擎
│   ├── JoltPhysics-master/     #   物理引擎
│   ├── HFSM2-2.12.1/           #   层次状态机
│   ├── miniaudio-master/       #   音频
│   ├── nlohmann/               #   JSON
│   ├── sqlite/                 #   本地存储
│   ├── log/                    #   spdlog 封装
│   └── googletest-main/        #   测试框架（可选）
│
└── 通用开发记忆.md             # 跨项目通用开发笔记
```

---

## 二、双可执行目标

```
       共享引擎核心（ENGINE_SOURCES）
   Core / Components / Systems / Scripting / Render
        │                                    │
   ┌────┴─────────────┐          ┌───────────┴──────────┐
   │  my_demo.exe     │          │   my_game.exe        │
   │  （编辑器）       │          │   （游戏运行时）      │
   │  + editor/       │          │   + Game/GameWindow  │
   │  三栏UI/脚本编辑器 │          │   只有视口，启动即Play │
   │  /导出按钮        │          │   给玩家的成品        │
   └──────────────────┘          └──────────────────────┘
```

---

## 三、环境准备

| 组件 | 版本 / 要求 |
| ------ | ------------ |
| 操作系统 | Windows 10 / 11 |
| Qt | 5.14.2 (msvc2017_64)，安装在 `D:/QT/5.14.2/msvc2017_64` |
| 编译器 | MSVC 2019 / 2022 (x64) |
| CMake | 3.15+ |
| 显卡驱动 | 支持 OpenGL 3.3 Core Profile |

> Qt 路径在 `CMakeLists.txt` 第 29 行配置，换路径改这一处即可。

---

## 四、构建与运行

```powershell
# 配置（生成 VS2022 工程）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 构建全部（Release）
cmake --build build --config Release

# 运行编辑器
./build/Release/my_demo.exe

# 单目标构建
cmake --build build --config Release --target my_game
cmake --build build --config Release --target lesson05_texture
```

### 构建目标一览

| 目标 | 说明 |
| ------ | ------ |
| `my_demo` | 编辑器（做游戏用） |
| `my_game` | 游戏运行时（导出的成品） |
| `reflect_gen` | 反射代码生成器（构建自动执行） |
| `unit_tests` | 单元测试（需放置 googletest 源码） |
| `integration_tests` | 集成测试（物理/脚本/粒子/动画/补间/场景/资源） |
| `engine_tests` | 无头引擎测试 |
| `lesson01` ~ `lesson21` | OpenGL 学习课程（各自独立可运行） |

---

## 五、OpenGL 学习课程

21 节渐进式课程，从窗口创建到延迟渲染，每节独立可运行。
详见 [lessons/ 目录](lessons/) 和 [课程 README](lessons/README.md)。

| # | 课程 | 核心知识点 |
| --- | ------ | ----------- |
| 01 | Hello Window | QOpenGLWidget + 清屏 |
| 02 | Hello Triangle | VBO/VAO + 着色器 |
| 03 | Shader Uniform | uniform 变量 + Shader 工具类 |
| 04 | EBO Rectangle | 索引缓冲 + 线框模式 |
| 05 | Texture | 纹理贴图 + 混合 |
| 06 | Transform | 矩阵变换 + 动画 |
| 07 | 3D Coordinate | MVP 矩阵 + 深度测试 |
| 08 | Camera | FPS 摄像机 + WASD |
| 09 | Basic Lighting | Phong 光照模型 |
| 10 | Material | 材质属性 |
| 11 | Lighting Maps | 漫反射/镜面光贴图 |
| 12 | Light Casters | 方向光/点光源/聚光灯 |
| 13 | Multiple Lights | 多光源混合 |
| 14 | Framebuffer | FBO + 后处理特效 |
| 15 | Cubemap | 立方体贴图 + 天空盒 |
| 16 | Instancing | 实例化渲染 |
| 17 | Shadow Mapping | 阴影映射 + PCF |
| 18 | Normal Mapping | 法线贴图 |
| 19 | Bloom | HDR + 泛光 |
| 20 | Geometry Shader | 几何着色器 |
| 21 | Deferred Shading | 延迟渲染 |

---

## 六、引擎文档导航

| 文档 | 给谁看 | 内容 |
| ------ | -------- | ------ |
| [模块文档.md](docs/模块文档.md) | 想理解/扩展引擎的人 | 架构、模块职责、数据流、设计模式 |
| [脚本API手册.md](docs/脚本API手册.md) | 用引擎做游戏的人 | 全部 Lua API + 三种开发方式 + 最小示例 |
| [排障手册.md](docs/排障手册.md) | 遇到 bug 的人 | 实战坑 + 排障方法论 |
| [功能更新计划.md](功能更新计划.md) | 规划功能的人 | 待实现功能路线图 |

---

## 七、依赖版本

| 库 | 版本 | 用途 |
| ---- | ------ | ------ |
| Qt | 5.14.2 | 窗口/输入/OpenGL 上下文 |
| flecs | 4.1.6 | ECS 框架 |
| GLM | 1.0.3 | 数学库 |
| Lua | 5.4.7 | 脚本引擎 |
| Jolt Physics | master | 物理引擎 |
| HFSM2 | 2.12.1 | 层次状态机 |
| miniaudio | master | 音频 |
| nlohmann/json | — | JSON 序列化 |
| sqlite | — | 本地存储 |
| GoogleTest | main | 测试框架（可选） |

---

## 八、编码约定

- C++20，UTF-8 编码，MSVC 强制 `/utf-8 /FS`
- 类名 PascalCase，函数 camelCase，成员 `m_` 前缀
- RAII + 智能指针，禁止裸 new
- 函数 ≤ 50 行，单一职责
- 详细规范见 [AGENTS.md](AGENTS.md) 和 [.qoder/rules/](.qoder/rules/)
