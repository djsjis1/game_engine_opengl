---
trigger: always_on
---
# Git 工作流规范

## 本文件定位

本文件定义 Commit 格式、分支策略与 PR 规范，AI 每次提交均须遵守。

与 `documentation_rules.md` 对齐：Commit 信息统一使用中文。

---

# Commit 格式

使用 `<type>(<scope>): <subject>` 格式：

```
feat(timeline): 新增移动操作的撤销重做支持

- 实现 MoveCommand 的 execute/undo
- 撤销栈深度限制为 kMaxUndoDepth

Refs: TASK-001, ADR-001
```

## type 取值

| type | 含义 |
| ------ | ------ |
| feat | 新功能 |
| fix | Bug 修复 |
| refactor | 重构（既不新增功能也不修复） |
| perf | 性能优化 |
| docs | 仅文档变更 |
| test | 仅测试变更 |
| chore | 构建 / 脚本 / 依赖维护 |
| revert | 回滚 |
| wip | 进行中保存点（仅 feature/fix 分支，合入前必须 squash 整理） |

## 规则

* subject 使用中文，不超过 50 字，使用祈使句
* 一次提交只做一件事（原子性），禁止混合多个职责
* 涉及 TASK 或 ADR 的提交，必须在 footer 用 `Refs: TASK-XXX, ADR-XXX` 关联
* 禁止提交 `WIP`、`temp`、`fix bug` 等无意义信息（例外：会话中断时的 `wip` 保存点，见「AI 提交规则」）
* 禁止提交构建产物、临时文件（按 `project_structure_rules.md` 的 .gitignore 规范）

---

# 分支策略

| 分支 | 用途 | 规则 |
| ------ | ------ | ------ |
| `main` | 发布版本 | 禁止直接 push，只接受 release 合入 |
| `develop` | 开发主干 | 日常集成分支 |
| `feature/TASK-XXX-描述` | 新功能 | 从 develop 拉出，完成后合回 develop |
| `fix/TASK-XXX-描述` | 缺陷修复 | 同上 |
| `release/vX.Y.Z` | 发布准备 | 从 develop 拉出，只允许修复不允许新功能 |

功能分支命名必须包含 TASK 编号，保证可追溯。

---

# PR / MR 规范

合入 `develop` / `main` 必须通过 PR，且满足：

* 标题符合 Commit 格式
* 描述包含：动机、变更点、测试情况、关联的 TASK / ADR
* CI 全绿（格式、静态分析、编译、测试、Sanitizer，见 `cpp_project_rules.md`）
* Review 人数：合并前至少 1 人；涉及公共接口变更、并发相关、安全敏感（输入解析 / 权限控制）、性能关键路径的变更至少 2 人
* 禁止自己批准合入自己的 PR，禁止带红灯合并

---

# Tag 与版本

* 使用语义化版本：`vX.Y.Z`
* 主版本 X：API / ABI 不兼容变更
* 次版本 Y：向后兼容的功能新增
* 修订号 Z：向后兼容的缺陷修复
* 每个 Tag 必须附发布说明（变更摘要 + 影响范围）

---

# AI 提交规则

## Commit 粒度：逻辑变更，而非文件操作

* 一次 commit 对应一个完整的逻辑变更，可包含多个文件的增删改（如「撤销重做」= 新增 2 文件 + 修改 3 文件 = 1 个 commit）
* 禁止每改一个文件就 commit 一次（碎片化）；禁止把多个不相关的逻辑变更塞进一个 commit（混合提交）
* 每个 commit 必须能独立通过构建与测试（bisect 友好），禁止提交半成品

频率参考：

| 变更级别 | 建议 commit 数 | 说明 |
| ---------- | ---------------- | ------ |
| S | 1 | 一个逻辑变更一次 commit |
| M | 1~3 | 按独立子步骤拆分，每个可独立构建 |
| L | 每个子任务 1 个 | 与 TASK 子任务对应，完成后统一 PR |

## Commit 前审查（强制）

AI 自动 commit 前必须执行，发现问题先修复再提交：

1. `git status`：确认变更清单与任务预期一致，无意外文件（误格式化、临时文件、构建产物）
2. `git diff`（逐文件）：审查每个文件的实际改动，确认没有夹带无关修改
3. 删除文件：在提交信息与完成确认中单独列出；属于破坏性操作，须先征得用户确认（见 `engineering_rules.md`）
4. 暂存按文件分组 `git add <file>`，禁止无条件 `git add -A` 把无关文件卷进来

## 触发时机

* 一个逻辑变更完成且 DoD 验证通过（构建 / 测试 / 静态检查全部通过）后，AI 自动执行 `git add` + `git commit`
* S 级变更可在简化自检通过后直接 commit
* Commit 信息必须符合本文件全部格式要求

## 保存点（会话中断时）

* 逻辑变更未完成但会话即将中断（上下文耗尽 / 用户暂停）时，允许一次保存点 commit：type 用 `wip`，如 `wip(timeline): 撤销栈实现进行中`
* `wip` 仅允许出现在 feature / fix 分支；develop / main 上禁止 `wip`
* 恢复开发后：同一逻辑变更的 wip 与正式 commit 在合入前用 squash 整理为一个正式 commit（见下节）

## 历史整理

* 未 push 的本地 commit 可由 AI 用 `git rebase -i` squash / fixup 整理为干净的逻辑提交
* push 之后的历史禁止改写（rebase、reset、force push），除非用户明确同意

## 自动分支策略

AI 在 commit 前自动判断是否需要创建分支：

| 条件 | 操作 |
| ------ | ------ |
| S 级变更，当前为 main/develop | 创建 `feature/描述` 分支后 commit |
| S 级变更，当前为 feature/fix | 直接在当前分支 commit |
| M/L 级变更 | 自动创建 `feature/TASK-XXX-描述` 或 `fix/TASK-XXX-描述` 分支后 commit |
| 已在 feature/fix 分支上 | 直接 commit，不重复创建 |

分支命名规范不变：`feature/TASK-XXX-描述` 或 `fix/TASK-XXX-描述`，无 TASK 编号时用简短描述命名。

## 用户信息

```
user.name = xiefujun
user.email = 853240265@qq.com
```
