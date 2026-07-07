---
trigger: always_on
---
# Git 规范

## 核心原则

Git 历史是项目的「第二文档」。

提交历史必须可读、可追溯、可审计。

禁止 `update` / `fix` / `modify` 等无意义提交。

---

# Commit Message 规范

## 格式

采用 Conventional Commits 规范：

```
<类型>(<范围>): <描述>

[可选正文]

[可选脚注]
```

类型定义：

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(motion): 新增速度倍率控制` |
| `fix` | Bug 修复 | `fix(protocol): 修复浮点数解析精度丢失` |
| `refactor` | 重构（不改变功能） | `refactor(timeline): 拆分 Command 子类` |
| `perf` | 性能优化 | `perf(decodec): 使用对象池减少内存分配` |
| `test` | 测试相关 | `test(motion): 新增运动控制集成测试` |
| `docs` | 文档更新 | `docs(adr): 新增协议选择决策记录` |
| `build` | 构建系统变更 | `build(cmake): 启用 ASAN 检测` |
| `ci` | CI/CD 配置变更 | `ci(github): 新增 clang-tidy 检查` |
| `chore` | 其他杂项 | `chore(deps): 升级 FFmpeg 到 6.1` |
| `style` | 格式变更（不影响逻辑） | `style: clang-format 批量格式化` |

范围（scope）使用模块名，低层级使用 snake_case：

```
feat(motion):       ← 模块
feat(motion,timeline):  ← 多模块
fix(motion_controller): ← 具体类
```

## 完整示例

```
feat(motion): 新增速度倍率控制

通过 MotionConfig::speed_ratio 控制运动速度倍率，
范围 0.1x ~ 10.0x，默认 1.0x。

支持通过 UI 滑块实时调整。

关联 TASK-012
关联 ADR-005
```

## 禁止的提交信息

禁止：

```
update          ← 更新了什么？
fix             ← 修复了什么？
modify          ← 修改了什么？
some changes    ← 什么变化？
wip             ← 不完整的工作
temp            ← 临时提交
test            ← 测试什么？
debug           ← 调试什么？
```

---

# 分支策略

## 分支命名

```
main                  ← 稳定发布
develop               ← 开发集成分支
feature/<模块>-<描述>   ← 功能分支
hotfix/<版本>-<描述>    ← 紧急修复
release/<版本>         ← 发布准备
```

示例：

```bash
feature/motion-speed-control
feature/timeline-undo-redo
hotfix/v2.1-fix-float-parse
release/v2.2
```

## 分支管理规则

**main 分支**：

* 禁止直接 push
* 只接受 PR 合并（Squash merge 或 Merge commit）
* 每次合并对应一个版本号
* 合并后必须打 tag

**develop 分支**：

* 所有 feature 分支从此分出
* feature 完成后合并回此分支
* 禁止直接 push（通过 PR 合并）

**feature 分支**：

* 命名：`feature/<模块>-<简短描述>`
* 生命周期结束后必须删除（禁止长期保留）

---

# PR / MR 规范

## PR 标题

格式同 commit message：

```
feat(motion): 新增速度倍率控制
```

## PR 描述模板

```markdown
## 变更概述

[一句话描述]

## 变更原因

[为什么做这个变更]

## 变更清单

### 新增文件
- path/to/new_file.h
- path/to/new_file.cpp

### 修改文件
- path/to/file.h

### 删除文件
- 无

## 测试

- [x] 单元测试通过
- [x] 集成测试通过
- [x] ASAN 无泄漏
- [ ] 需要手动测试（描述如何测试）

## 文档

- [x] docs/api.md 已更新
- [x] docs/changelog.md 已更新

## 关联

- 关联 TASK-XXX
- 关联 ADR-XXX
```

## 合并要求

* 至少 1 人 Review 通过
* CI 全部绿色
* 无合并冲突
* 关联的 TASK 状态已更新

---

# AI 提交建议

AI 在 Step 7（最终确认）时必须输出建议的 commit message：

```bash
## 建议 Commit

git commit -m "feat(motion): 新增速度倍率控制

通过 MotionConfig::speed_ratio 控制运动速度倍率，
范围 0.1x ~ 10.0x。

关联 TASK-012"
```

禁止 AI 执行 git commit（应在用户审核后手动执行）。

---

# Tag 与版本规范

版本号遵循语义化版本 `主版本.次版本.修订号`：

```
v1.0.0    ← 初始发布
v1.1.0    ← 新增功能（向后兼容）
v1.1.1    ← Bug 修复
v2.0.0    ← 不兼容的 API 变更
```

每次发布必须：

1. 在 main 分支打 tag
2. 更新 `docs/changelog.md`
3. 更新 `README.md` 版本号

---

# 禁止行为

禁止：

* 提交调试代码（`printf`、`std::cout`、`TODO: remove`）
* 提交未格式化的代码（必须先 `clang-format`）
* 提交临时文件（`.tmp`、`.bak`、`~` 结尾的文件）
* 一个 commit 包含多个不相关的修改
* 在 commit 中提交 binary 文件（库文件、可执行文件，除非是发布专用仓库）
* `git push --force` 到 main 或 develop 分支
* 提交包含密钥、token、密码的文件
* 跳过 CI 检查强行合并

---

# .gitignore 审查

每次新增构建产物、临时文件类型时必须检查 `.gitignore` 是否覆盖。

参见 `project_structure_rules.md` 中的 `.gitignore` 模板。

---

# 最终检查

✓ Commit message 符合 `类型(范围): 描述` 格式

✓ 一个 commit 只做一件事

✓ 分支命名符合规范

✓ PR 描述填写完整

✓ 无调试代码残留

✓ CI 全部通过

✓ 已打 tag（如果是发布）
