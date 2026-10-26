# ADR-002：信号队列下沉 Core 层，修复 Systems→Scripting 反向依赖

## 背景

项目分层约定：`Core`（引擎基础，无 UI/脚本依赖）← `Systems`（逻辑系统）←
`Scripting`（Lua 层）。但代码审查发现两处反向依赖：

- `Systems/PhysicsEngine.cpp` include `Scripting/LuaApi.h` —— 物理引擎
  （底层、Jolt 工作线程回调）依赖脚本层，只为推碰撞信号到 `g_signalQueue`
- `Systems/AnimationSystem.h` include `Scripting/LuaApi.h` —— 动画系统
  （纯数据驱动）依赖脚本层，只为访问 `g_signalQueue`/`g_signalMutex`
- `Systems/TweenSystem.cpp` 同样 include（补间完成信号）

## 问题

1. **编译耦合**：LuaApi.h 拉入 lua.h/lauxlib.h 等重头文件，任何 LuaApi
   改动都触发物理/动画/补间系统重编译；头文件依赖链变深。
2. **依赖方向反了**：底层系统知道高层脚本的存在，违反"模块间通过接口
   通信，避免硬依赖"的项目规范；未来做无脚本的 headless 构建时，物理/
   动画会被迫拉入 Lua。
3. **重复样板**：三处生产者各自写 `lock_guard + g_signalQueue.push`，
   加锁逻辑复制三份。

## 备选方案

### 方案 A：抽 Core/SignalQueue.h 数据层（队列仍 extern）

- `DeferredSignal` 结构 + `g_signalQueue`/`g_signalMutex` extern 声明 +
  `clearDeferredSignals()` + 新增统一投递入口 `pushDeferredSignal()`
  迁到 `Core/SignalQueue.h/.cpp`（零 Lua 依赖）
- LuaApi.h include 它保持兼容；`processDeferredSignals`（需 lua_State）
  留在 Scripting 层消费
- 优点：改动小、依赖方向摆正、消灭三处锁样板；extern 队列保留使集成
  测试可直接检视队列内容（TweenSystem 测试依赖这一点）
- 缺点：全局变量仍 extern（封装不彻底）

### 方案 B：完全封装接口（不暴露队列）

- Core 只暴露 `push/clear/tryPopAll`，队列与锁全私有
- 优点：封装最彻底
- 缺点：测试需直接检视队列（front/pop 断言），要额外加 peek 测试专用
  接口——为测试开洞反而更丑；改动面大、收益有限

## 最终方案

**方案 A：Core/SignalQueue.h/.cpp 数据层下沉 + 统一投递入口**

## 选择原因

1. 队列本质是"C++ 系统 → 脚本"的跨层总线，数据定义零 Lua 依赖，天然
   属于 Core 层；消费逻辑（processDeferredSignals）因需要 lua_State
   留在 Scripting，生产/消费按能力分层，方向单一指向 Core。
2. `pushDeferredSignal()` 统一入口把三处"加锁+push"样板收敛为一行调用，
   未来换锁策略（如无锁队列）只改一处。
3. extern 保留是务实取舍：集成测试对队列内容的直接断言是现有测试资产，
   方案 B 为封装付出的测试接口成本不值。
4. LuaApi.h 保留 include 兼容，既有调用方（engine_test_main 等）零改动。

## 影响范围

- 新增 `Core/SignalQueue.h` / `Core/SignalQueue.cpp`（结构体/队列/锁/
  push/clear）
- `Scripting/LuaApi.h`：移除 DeferredSignal/clearDeferredSignals/队列
  extern 声明，改 include Core/SignalQueue.h（兼容）
- `Scripting/LuaApi.cpp`：移除队列/锁定义与 clearDeferredSignals 实现，
  enqueueSignal 改用 pushDeferredSignal
- `Systems/PhysicsEngine.cpp`、`Systems/AnimationSystem.h`、
  `Systems/TweenSystem.cpp`：改 include Core/SignalQueue.h，推送点改
  pushDeferredSignal（物理/动画/补间不再依赖 Scripting）
- `MainWindow.cpp`：clearDeferredSignals 改从 Core/SignalQueue.h 引入
- `tests/integration/tween/TweenSystem_test.cpp`：改 include Core 头
- `CMakeLists.txt`：ENGINE_SOURCES 注册两个新文件

## 验证结果

my_demo / my_game / unit_tests / integration_tests / engine_tests 全部
编译通过无警告；unit_tests 69 用例、integration_tests 98 用例全绿
（含 TweenSystem 信号队列断言——行为零回归）。

## 修订（2026-10-26）：收编 extern 全局变量

原决策保留 `g_signalQueue`/`g_signalMutex` extern（方案 A），理由是
"测试需直接检视队列"。后续项目自查发现这与模块解耦铁律（头文件只暴露
接口、状态收进匿名命名空间）冲突：任何 include 者都能绕过锁直接操作
裸队列，锁纪律只靠自觉维持。

实际修订：

- 队列与锁移入 `SignalQueue.cpp` 匿名命名空间（文件私有）
- 新增 `takeDeferredSignals()`：加锁 swap + 转移到 vector 返回，一个接口
  覆盖两个需求——Scripting 每帧消费、测试断言（取出后检视）
- 消费方 `LuaApi.cpp` 与 `TweenSystem_test.cpp` 均已迁移，行为零变化
- 头文件接口收敛为 push / take / clear 三个函数，无任何数据暴露

当时"为测试开洞更丑"的顾虑不成立：take 语义（原子取走全部）正是消费方
本来要做的事，测试复用它不需要额外的 peek 接口——当初方案 B 的缺点评估
有误，修正之。

## 状态

已采纳（2026-08-13）；2026-10-26 修订：extern 收编为文件私有（接口
push/take/clear，全封装）
