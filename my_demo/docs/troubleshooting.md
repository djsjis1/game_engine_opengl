# 排障手册（Troubleshooting）

> 实战踩坑记录：每条按 **症状 → 根因 → 修复 → 快速定位** 组织。
> 更早的坑 1~14（HFSM2 拷贝、Flecs get 崩溃、WIN32 无控制台等）见 `../MODULES.md` 第八节。
> 本文件记录「多场景游戏 + 导出」实战期间（坑 15 起）抓出的问题。

---

## 坑 15：手写场景 JSON 全部字段变 0（"场景里只有一个盒子"）

**症状**：加载场景后所有实体挤在原点重叠（看着像只有一个盒子）、玩家控制不了、相机位置错乱。

**根因**：flecs JSON 反序列化要求它自己的嵌套格式，手写的两种直觉格式都解析失败且**静默跳过**（字段全部保持默认值 0）：

```json
手写（都错）：                     flecs 实际要的：
"position": [0, 3, 0]              "position": [{ "x": [0], "y": [3], "z": [0] }]
"position": {"x":0, "y":3, "z":0}
```

**修复**：场景文件**永远用引擎自己生成**（编辑器 💾 保存 / `SceneSerializer::save` / test_gameplay 程序化建场景后 save），绝不手写。

**快速定位**：控制台版程序加载场景后打印实体 position——若明明配了非零值却读出 (0,0,0)，且 stderr 有 `flecs: expected '['` / `unsupported conversion`，就是格式问题。

---

## 坑 16：编辑器点 ▶ 运行后 WASD 动的是相机不是角色

**症状**：Play 模式按 W，视角飞走，游戏角色不动；看到的也不是游戏相机的画面。

**根因**：编辑器相机（isEditor=true, active=true）在 Play 时仍然激活——CameraSystem 继续用 WASD 驱动它，RenderWidget 也优先用它渲染。

**修复**（MainWindow onEnterPlay）：场景里有游戏相机时，把编辑器相机 `active=false` 让位；退出 Play 恢复。CameraSystem 只驱动"激活的编辑器相机"（`isEditor && active`）。

**快速定位**：按 W 时若右键环视的视角在动 → 编辑器相机还在吃输入。

---

## 坑 17：站一会儿之后推不动 / set_velocity 失效

**症状**：角色落地静止约半秒后，WASD/冲量全部失效，像被冻住。

**根因**：Jolt 刚体静止后自动**睡眠**（省性能），而 `SetLinearVelocity` / 部分操作**不会唤醒**睡眠刚体。

**修复**：`physicsSetVelocity` / `physicsApplyImpulse` 先 `ActivateBody` 再操作。

**通用规律**：所有物理引擎（Jolt/PhysX/Bullet）都有睡眠机制——**任何"从外部改睡眠刚体"的操作都要先唤醒**。

---

## 坑 18：传送门第一次切错场景，第二次才对

**症状**：第一次走进传送门"被传回原点"（其实是重载了当前场景），第二次才切到正确场景。

**根因**：编辑器 📂 加载场景时没有登记"当前场景名"（SceneRouter::current 为空串）→ 脚本 `get_scene()` 返回 ""，`string.find("", "level1")` 为 nil → 走错分支切到自己。第一次切换顺带把 current 设对了，所以第二次正常。

**修复**：所有加载场景的入口（编辑器加载按钮 / GameWindow / Play 帧末切换 / 退 Play 恢复备份）都必须同步 `SceneRouter::setCurrent`。

**通用规律**："当前 XX"这类全局状态，**每个改变它的入口都要同步更新**，漏一个入口就出"第一次错第二次对"的诡异现象。

---

## 坑 19：落地检测射线的起点两难（跳跃判定失效）

**症状**：空格跳不了（落地检测永远 false）；或射线永远命中（打中自己）。

**根因**：射线起点放哪都不对——放自己碰撞体内部会命中自己（Jolt 对 convex 内部起点返回 fraction=0 命中）；挪出自己盒子又容易扎进地面内部（从地面形状内部发射 → 不命中地面）。

**修复**：不调坐标，用**过滤器**——`physicsRaycast` 加 `ignoreEntity` 参数（`JPH::IgnoreSingleBodyFilter`），Lua `raycast` 自动忽略发射者。射线放心从自身中心发。

**通用规律**：Godot `intersect_ray` 的 exclude 同款语义——"射线不打自己"要靠 BodyFilter，不靠起点微调。

---

## 坑 20：按空格弹开工具栏下拉框（游戏按键被 UI 吃掉）

**症状**：Play 模式按空格，编码下拉框弹开；W/S 触发控件首字母匹配。

**根因**：三层叠加——

1. QComboBox 默认接受键盘焦点，空格是它的激活键；
2. app 级 eventFilter `return false` 只是"偷看"，不阻止事件继续传给焦点控件；
3. `QOpenGLWidget` 默认 `NoFocus`，对它调 `setFocus()` 是**空操作**（焦点根本移不过去）。

**修复**：① 工具栏输入控件 `setFocusPolicy(Qt::NoFocus)`；② 视口 `setFocusPolicy(Qt::StrongFocus)` 后进 Play `setFocus()`；③ 根治：Play 模式下 KeyPress/KeyRelease 在 Input 记录后 `return true` 拦截，不再传播给任何 UI。

**通用规律**：拿 Qt 做游戏引擎，**运行模式必须明确切断"按键→UI 控件"的传播**，否则游戏按键和控件快捷键必然打架。

---

## 坑 21：读档/重生瞬移失败（set_position 被拉回原地）

**症状**：读档后角色纹丝不动；掉落重生无效——但存档文件内容是对的。

**根因**：带 Dynamic 刚体的实体，位置的"真身"在物理引擎里。`set_position` 只写 Transform，下一帧 PhysicsSystem 用刚体位置回写 Transform → 改动被覆盖。

**修复**：`set_position` / `set_node_position` 双通道——写 Transform（渲染立即生效）+ `physicsSetPosition`（Jolt `SetPosition` + Activate，物理真身一起搬）。

**通用规律**：**谁拥有数据的"真身"，写操作就必须写到谁那里**。物理接管后 Transform 只是每帧的抄写员：读它没问题，写它无效。

---

## 坑 22：跳跃"完全失效"——方块默认重达一吨（本轮最深的坑）

**症状**：`apply_impulse(0,6,0)` 毫无反应；但移动、重力、碰撞全部正常。

**根因**：建 Jolt 物理体时**没把 `RigidbodyComponent.mass` 传过去**。Jolt 收不到质量就按"体积 × 默认密度 1000kg/m³"自动计算 → 1×1×1 方块 = **1000 公斤**。冲量 6 只产生 Δv = 6/1000 = 0.006 m/s——跳跃"生效着"，只是慢到看不见。

**为什么其它全正常（藏得深的原因）**：

- 重力加速度与质量无关 → 掉落正常；
- `set_velocity` 直接设速度、绕过质量 → 移动正常；
- 只有"力/冲量"类操作被千倍质量吞掉。

**修复**：`BodyCreationSettings.mOverrideMassProperties = CalculateInertia` + `mMassPropertiesOverride.mMass = rb.mass`。

**快速定位**（数值反推法）：impulse 后读 `get_velocity`，用 Δv = J/m 反推实际质量。本例 vy=0.01 → m ≈ 6/0.006 = 1000kg，一个数字破案。

**通用规律**：**组件里配置字段存在 ≠ 配置生效**。加新字段时检查清单必须多一条："消费端（物理/渲染/序列化）真的读了这个字段吗？"这类"断线"bug 无报错、大部分功能正常，最难发现。

---

## 排障方法论（比单个坑更重要）

本轮全部问题都靠同一套打法破案，沉淀如下：

### 1. 可视化诊断（WIN32 无控制台的现实）

GUI 程序里 `printf`/Lua `print` 全部不可见。诊断输出必须走用户看得见的通道：

- **状态栏**：`statusBar()->showMessage(...)` —— 验证 C++ 层事件；
- **实体颜色**：脚本里 `add_material(me, r, g, b)` 当调试灯 —— 验证 Lua 层分支；
- **位置编码**：探针脚本把结果写进自己的 y 坐标（111/222/333 分别代表不同状态）。

### 2. 分层探针（test_gameplay.exe，控制台版最小复现）

不开 GUI，程序化建场景 + 模拟按键 + 逐层断言：

```
[1] 物理体创建了吗（bodyId != 0）
[2] 重力生效了吗（y 在下降）——物理层独立验证
[3] 脚本推得动吗（按 W 后 z 变化）——输入→脚本→物理全链路
[4] 跳得起来吗（空格后 y 上升）
```

哪层 FAIL 病灶就在哪层与上一层之间。二分不猜。

### 3. 数值反推

拿到异常数值别只当"不正常"，**用公式反推出隐藏参数**：
vy=0.01 + 冲量 6 → 质量 1000kg → 直接联想到 Jolt 默认密度。数字会招供。

---

## 与其它文档的关系

- 坑 1~14（状态机/反射/编辑器/动画）：`../MODULES.md` 第八节
- 引擎模块职责与架构：`../MODULES.md` 前七节
- 修复对应的代码位置：每处都有 `实bug` / `实踩坑` 字样的现场注释，全局搜索可达
