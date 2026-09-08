# ADR-001：单元测试框架选 GoogleTest + 本地源码引入

## 背景

项目进入需要单元测试的阶段（纯逻辑模块 Input/Time/SceneRouter/Easing/
SaveGame 已稳定，需回归保护）。`testing_rules.md` 明确规定"统一使用
GoogleTest + GoogleMock"，且 `cpp_project_rules.md` 规定第三方库"优先
FetchContent 锁定版本，不得直接复制源码到项目（特殊情况需在 ADR 说明）"。

## 问题

1. 测试框架用什么？（规范已定 GoogleTest，无争议）
2. gtest 怎么引入？规范首选 FetchContent，但本机 github/gitee 均 SSL
   握手失败（schannel handshake failed），FetchContent 在 configure 阶段
   克隆仓库直接失败，阻断整个工程 configure。

## 备选方案

### 方案 A：FetchContent（规范首选）

- 优点：符合规范、版本锁定、无需提交库源码
- 缺点：**本机网络无法访问 github/gitee**，configure 阶段克隆失败 →
  整个工程无法生成，主工程也被拖垮。当前环境不可用。

### 方案 B：本地源码 add_subdirectory（与 flecs/lua/jolt 一致）

- 优点：零网络依赖；与项目现有第三方库（flecs-4.1.6/lua-5.4.7/
  JoltPhysics-master 全是本地源码）引入方式完全一致；带存在性检测，
  未放置库时自动跳过、不阻塞主工程
- 缺点：需手动放置 gtest 源码；库源码不进版本控制（.gitignore 已忽略
  thirdparty 大目录）

### 方案 C：自写轻量断言宏

- 优点：零依赖
- 缺点：违反"统一 GoogleTest"规范；重复造轮子；无 fixture/参数化等能力

## 最终方案

**方案 B：本地源码 add_subdirectory 引入 googletest-1.14.0**

## 选择原因

1. 规范的 FetchContent 在当前网络环境**客观不可用**（属规范例外条款
   允许的"特殊情况"），本 ADR 即为说明。
2. 与项目既有第三方库引入方式统一（thirdparty/ 下全是本地源码 +
   add_subdirectory），一致性优于个例。
3. 实际放置的是 googletest-main（含 gmock，版本 1.18.0）——受网络所限
   用手头可得的源码包，未纠结精确 tag；CMake 用 EXISTS 检测目录，
   换版本只改一处路径。
4. 带 `EXISTS` 检测：没放库时 configure 打印 skipped 并跳过 unit_tests
   目标，主工程构建完全不受影响——降低对未配置环境的侵入。

## 影响范围

- 新增 `tests/`：test_main.cpp + unit/core/ 下 5 个测试文件（45 用例）
- `CMakeLists.txt`：新增 unit_tests 目标（EXISTS 守卫）+ enable_testing
- `Core/Input.h`：新增 reset()（测试隔离 + 退 Play 清残留双用途）
- 依赖：thirdparty/googletest-main/（手动放置，同 flecs 等）
- 测试只覆盖 header-only 纯逻辑模块，不链 flecs/Jolt/OpenGL（目标轻、
  用例 <100ms 满足规范）

## 验证结果

`ctest -C Debug`：45 用例全通过（Input 20 / Time 6 / SceneRouter 6 /
Easing 7 / SaveGame 6），总耗时 ~51ms（满足单测 <100ms/用例）。

## 状态

已采纳（2026-07-01）
