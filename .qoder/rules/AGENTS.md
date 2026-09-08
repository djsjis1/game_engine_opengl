---
trigger: always_on
---
# AGENTS.md — AI 编码代理项目指令

> 本文件是跨工具通用入口（Codex / Copilot / Cursor / Windsurf / Amp 均识别）。
> 详细规则见 `.qoder/rules/` 目录。Claude Code 用户请 `ln -s AGENTS.md CLAUDE.md`。

## 技术栈

```
语言：C++20
构建：CMake 3.15+（target-based）
引擎：Flecs ECS 4.1.6 + Qt 5.14.2 + OpenGL 3.3 + Lua 5.4.7 + Jolt Physics + HFSM2 2.12.1
测试：GoogleTest v1.14.0
编译器：MSVC 2022，强制 UTF-8（/utf-8 /FS）
```

## 项目结构

```
Core/          — 引擎基础设施（Engine/Input/Time/SignalQueue/...）
Components/    — ECS 纯数据组件（REFLECT 宏自动反射）
Systems/       — 逻辑系统（Camera/Physics/Audio/Tween/Animation/...）
Scripting/     — Lua 脚本层（API 按域拆分 + 热重载）
Render/        — 渲染模块（多 Pass 管线：阴影/天空/后处理）
editor/        — 编辑器 UI（场景树/Inspector/脚本编辑器）
Game/          — 游戏运行时（导出后只有视口窗口）
assets/        — 资源（Shaders/scenes/scripts/anims）
tests/         — 测试（unit/ + integration/ + engine/）
lessons/       — OpenGL 渐进式学习课程（21 节，独立可运行）
docs/          — 引擎文档（模块文档/脚本API手册/排障手册/ADR）
tools/         — reflect_gen 反射代码生成器
thirdparty/    — 第三方库（源码内嵌，锁定版本）
```

## 开发命令

```powershell
# 配置
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 构建引擎
cmake --build build --config Release --target my_demo

# 构建游戏运行时
cmake --build build --config Release --target my_game

# 运行测试
cmake --build build --config Debug --target unit_tests
cmake --build build --config Debug --target integration_tests
```

## 关键边界

### 不可触碰

<!-- 红线：每条针对一个代价高昂的错误，空白即未配置 -->

### 不可修改的目录/文件

- `Generated/*.inl` — reflect_gen 自动生成，永不手改
- `lessons/` — OpenGL 学习课程，独立于引擎，不改其代码

## 代码约定速查

* 每个文件头部有注释：文件职责 + 关键接口 + 用法示例
* 命名表达真实含义，禁止 `tmp` / `data` / `obj` 等无意义命名
* 注释解释「为什么」，不解释「做什么」
* 错误必须处理，禁止空 catch / 静默失败
* 资源一律 RAII / 智能指针，禁止裸 `new`
* 模块间通过接口 / 依赖注入通信，禁止直接引用内部实现
* 新增功能先判断能否复用已有模块
* 无法自动化的路径（UI 点击 / 硬件 / 时序）留 `testHook_` 测试钩子自测，宏单点开关（见 cpp_project_rules.md）

## 变更流程

| 级别 | 判定 | 做法 |
| ------ | ------ | ------ |
| S | 单文件、< 30 行 | 说明意图 → 直接改 → 简化自检 → 自动 commit |
| M | 单模块、< 300 行 | 需求分析 → 影响范围 → 编码 → 自检 → 最终确认 → 自动 commit |
| L | 跨模块 / 公共接口 | 完整设计（双方案比较）→ 编码 → 自检 → 自动 commit |

## Git

* 用户：`xiefujun <853240265@qq.com>`
* Commit 格式：`type(scope): 中文描述`，一次提交一件事
* Commit 粒度：一次 commit = 一个逻辑变更（可含多文件增删改），禁止每文件一 commit；S/M/L ≈ 1 / 1~3 / 每子任务 1 个
* 会话中断时允许 `wip` 保存点（仅 feature/fix 分支），合入前 squash 整理；commit 前必须审查 `git status` / `git diff`
* 分支：S 级直接 commit（在 main/develop 上时先建 `feature/描述` 分支）；M/L 级自动创建 `feature/TASK-XXX-描述` 分支
* 禁止改写已 push 的历史（rebase / reset / force push）除非用户同意

## 详细规则索引

| 场景 | 加载哪份规则 |
| ------ | ------------- |
| 编辑 C++ 源文件 | `cpp_project_rules.md`（自动） |
| 编写 / 修改测试 | `testing_rules.md` |
| 提交代码 | `git_rules.md`（常驻） |
| 新建模块 / 目录 | `project_structure_rules.md` |
| 多步骤任务 | `task_management_rules.md` |
| 写 / 更新文档 | `documentation_rules.md` |
| 重大设计决策 | `architecture_decision_record.md` |
| 完整工作流 | `ai_workflow_rules.md`（常驻） |
| 工程总纲 | `engineering_rules.md`（常驻） |
