# 03 · Context 与数据共享

> 状态间如何共享数据、ContextT 配置、引用语义与值语义的区别。

---

## 3.1 问题：状态之间怎么通信

状态 A 的 enter 里算了一个值，状态 B 的 update 里要用——怎么传？

```cpp
// ❌ 全局变量（丑、不安全）
int g_score = 0;

// ❌ 静态成员（耦合）
struct Playing { static int score; };
```

HFSM2 的解法：**Context**——一个所有状态共享的数据对象。

---

## 3.2 定义 Context

```cpp
// 1. 定义共享数据结构
struct GameContext {
    int score = 0;
    float deltaTime = 0;
    bool shouldQuit = false;
};

// 2. 在 Config 中指定 Context 类型
using M = hfsm2::MachineT<hfsm2::Config::ContextT<GameContext>>;

// 3. 定义状态机
using FSM = M::PeerRoot<struct Menu, struct Playing>;
```

---

## 3.3 在状态中访问 Context

```cpp
struct Playing : FSM::State {
    void enter(Control &control) {
        // 通过 control.context() 访问共享数据
        control.context().score = 0;
    }

    void update(Control &control) {
        auto &ctx = control.context();
        ctx.score += 10;  // 每帧加分

        if (ctx.shouldQuit)
            control.changeTo<Menu>();
    }
};

struct Menu : FSM::State {
    void enter(Control &control) {
        // 读取 Playing 留下的 score
        std::cout << "最终得分: " << control.context().score << "\n";
    }
};
```

---

## 3.4 创建带 Context 的状态机实例

```cpp
int main() {
    GameContext ctx;
    ctx.deltaTime = 1.0f / 60.0f;

    // 方式 1：传入 Context 对象（值语义：拷贝一份）
    FSM::Instance machine(ctx);

    // 方式 2：传入引用（引用语义：共享同一份）
    // 需要在 Config 中用 ContextT<GameContext &>
}
```

---

## 3.5 值语义 vs 引用语义（重要陷阱）

### 值语义（默认）

```cpp
using M = hfsm2::MachineT<hfsm2::Config::ContextT<GameContext>>;
//                                                      ^^^^^^^^^^^^
//                                                      值类型 = 拷贝

GameContext ctx;
FSM::Instance machine(ctx);  // machine 内部拷贝了一份 ctx
ctx.score = 100;             // 改外面的 ctx → machine 内部看不到！
```

**问题**：构造后外部修改 ctx，状态机内部看不到（因为是拷贝）。

### 引用语义（推荐用于外部注册回调的场景）

```cpp
using M = hfsm2::MachineT<hfsm2::Config::ContextT<GameContext &>>;
//                                                              ^
//                                                         引用 = 共享

GameContext ctx;
FSM::Instance machine(ctx);  // machine 内部持有 ctx 的引用
ctx.score = 100;             // 改外面的 ctx → machine 内部看得到 ✅
```

### 选择建议

| 场景 | 选择 |
|------|------|
| Context 只在状态机内部使用 | 值语义（简单安全） |
| 外部代码需要读写 Context（如注册回调） | **引用语义** |
| Context 含 `std::function` 回调，外部后续注册 | **必须引用语义**（否则回调注册到拷贝上，状态机看不到） |

---

## 3.6 引用语义的构造顺序要求

```cpp
struct Engine {
    GameContext m_ctx;                    // ① 先声明 Context
    FSM::Instance m_fsm{m_ctx};          // ② 后声明 FSM（持有 m_ctx 引用）
    // C++ 成员按声明顺序构造 → m_ctx 先于 m_fsm 构造 ✅
    // 析构按逆序 → m_fsm 先于 m_ctx 析构 ✅（引用不会悬空）
};
```

**如果顺序反了**（m_fsm 在 m_ctx 前面）：m_fsm 构造时 m_ctx 还没构造 → 引用悬空 → 未定义行为。

---

## 3.7 Context 中放回调（观察者模式）

```cpp
struct GameContext {
    // 状态切换时通知外部（如 UI 层）
    std::function<void()> onEnterPlay;
    std::function<void()> onExitPlay;
    std::function<void()> onEnterMenu;
};

struct Playing : FSM::State {
    void enter(Control &control) {
        if (control.context().onEnterPlay)
            control.context().onEnterPlay();  // 通知外部
    }
    void exit(Control &control) {
        if (control.context().onExitPlay)
            control.context().onExitPlay();
    }
};

// 外部注册回调（必须用引用语义 ContextT<GameContext &>）
GameContext ctx;
FSM::Instance machine(ctx);

ctx.onEnterPlay = []() { std::cout << "UI: 隐藏菜单\n"; };
ctx.onExitPlay = []() { std::cout << "UI: 显示菜单\n"; };
```

**这就是"状态机与外部解耦"的标准模式**：状态机不知道 UI 的存在，只调 Context 里的回调。

---

## 下一课

[04 · 转换、守卫与动作](./04-转换守卫与动作.md) — changeTo 的完整用法、条件转换、事件驱动。
