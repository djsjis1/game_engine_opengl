# Player — 项目说明

## 项目简介

（请补充项目一句话描述）

---

## 快速启动

（请补充构建和运行方式）

---

---

## 开发规范体系

本项目使用一套完整的 AI 驱动软件工程规范，位于 `.qoder/rules/` 目录下。

### 规范文件（9 份）

| 文件 | 用途 |
|------|------|
| `engineering_rules.md` | 软件工程总纲 — SOLID、架构、设计模式等通用原则 |
| `documentation_rules.md` | 文档体系 — 文档怎么写、何时更新、放哪里 |
| `project_structure_rules.md` | 项目结构 — 目录划分、文件命名、模块组织 |
| `task_management_rules.md` | 任务管理 — 任务跟踪、状态流转、需求拆分 |
| `testing_rules.md` | 测试体系 — 四类必测场景、覆盖率、用例规范 |
| `architecture_decision_record.md` | 架构决策 — 重要设计决策的书面记录 |
| `ai_workflow_rules.md` | AI 工作流 — AI 开发七步流程 |
| `cpp_project_rules.md` | C++ 工程实践 — 语言特性、工具链、构建系统 |
| `git_rules.md` | Git 规范 — Commit 格式、分支策略、PR 流程 |

### 如何使用

**对开发者：**

- 新增模块 → 查看 `project_structure_rules.md`
- 写测试 → 查看 `testing_rules.md`
- 提交代码 → 查看 `git_rules.md`
- 做架构决策 → 查看 `architecture_decision_record.md`
- 了解全貌 → 从 `engineering_rules.md` 开始

**对 AI Agent：**

所有规范文件设置 `trigger: always_on`，AI 每次对话自动加载全部 9 份规范，
按 [ai_workflow_rules.md](.qoder/rules/ai_workflow_rules.md) 定义的七步流程工作。

### 规则优先级

当规则冲突时：**项目专属规则 > 语言规则 > 工作流规则 > 工程总则**

详见 [engineering_rules.md](.qoder/rules/engineering_rules.md) 中的「规则冲突优先级」章节。

### 扩展项目专属规则

```
.qoder/rules/
├── ...（9 份通用规范，保持不变）
└── projects/                          ← 按需新增
    ├── motion_control_project_rules.md
    └── timeline_project_rules.md
```

---

## 项目目录结构

```
player/
├── src/                        # 源代码
├── include/                    # 公共头文件
├── tests/                      # 测试
├── docs/                       # 文档（架构、API、ADR、变更日志）
├── docs/tasks/                 # 任务跟踪
├── .qoder/rules/               # 开发规范（9 份）
├── scripts/                    # 构建脚本
├── tools/                      # 辅助工具
├── third_party/                # 第三方库
├── CMakeLists.txt
└── README.md                   # 本文件
```

---

## 文档

- 架构设计 → `docs/architecture.md`
- 模块说明 → `docs/modules.md`
- API 接口 → `docs/api.md`
- 变更日志 → `docs/changelog.md`
- 架构决策 → `docs/adr/`
- 任务跟踪 → `docs/tasks/`

---

## 依赖

（请补充项目依赖说明）

---

## 版本

（请补充版本信息）
