# 07 · Pipeline 与高级特性

> 自定义 Pipeline、Phase 排序、单例组件、配对查询、禁用组件、多线程。

---

## 7.1 Pipeline（管线）

Pipeline 定义了**系统的执行框架**。Flecs 内置一个默认 Pipeline，你也可以自定义。

```cpp
// 默认 Pipeline 的 Phase 顺序：
// OnLoad → PostLoad → PreUpdate → OnUpdate → OnValidate → PostUpdate → PreStore → OnStore

// 把系统放到特定 Phase
ecs.system<Physics>()
    .kind(flecs::OnUpdate)      // 在 OnUpdate 阶段执行
    .each([](Physics &p) { ... });

ecs.system<Render>()
    .kind(flecs::PostUpdate)    // 在 OnUpdate 之后执行
    .each([](Render &r) { ... });
```

---

## 7.2 自定义 Phase

内置 Phase 不够用时，可以创建自定义 Phase：

```cpp
// 创建自定义 Phase（在 OnUpdate 之后执行）
flecs::entity myPhase = ecs.entity("MyPhase")
    .add(flecs::Phase)
    .depends_on(flecs::OnUpdate);

// 系统放到自定义 Phase
ecs.system<Animation>()
    .kind(myPhase)
    .each([](Animation &a) { ... });
```

---

## 7.3 系统排序（同一 Phase 内）

同一 Phase 内的系统默认按创建顺序执行。可以显式指定依赖：

```cpp
flecs::entity sysA = ecs.system<A>().each([](A&) { ... });
flecs::entity sysB = ecs.system<B>()
    .depends_on(sysA)  // B 一定在 A 之后执行
    .each([](B&) { ... });
```

---

## 7.4 手动控制 progress

```cpp
// 自动模式：每次 progress 执行所有系统
ecs.progress(1.0f / 60.0f);

// 手动模式：禁用自动执行，自己控制
ecs.set_threads(0);  // 或自定义 pipeline
// 然后手动触发特定系统
```

---

## 7.5 禁用组件（Disabled）

实体可以临时"禁用"某些组件，查询时自动跳过：

```cpp
// 禁用组件（实体仍有这个组件，但查询时不参与）
e.disable<Enemy>();

// 查询所有 Enemy → 不会匹配到被禁用的
ecs.each<Enemy>([](Enemy &) {
    // 这里看不到被 disable 的实体
});

// 重新启用
e.enable<Enemy>();
```

**用途**：RPG 装备系统（卸下武器 = disable WeaponComponent，不删除数据）。

---

## 7.6 配对查询（Pair Query）

查询特定关系配对：

```cpp
struct Likes { };
struct Eats { };

alice.add<Likes>(bob);
alice.add<Eats>(apple);

// 查询 alice 的所有 Likes 目标
ecs.query_builder()
    .with<Likes>(flecs::Wildcard)  // 任意 target
    .each([](flecs::entity e, flecs::iter &it, size_t i) {
        flecs::entity target = it.pair(0).second();
        // target = bob
    });

// 查询谁 likes bob
ecs.query_builder()
    .with<Likes>(bob)  // 指定 target
    .each([](flecs::entity e) {
        // e = alice
    });
```

---

## 7.7 单例组件（Singleton）深入

```cpp
struct Time { float dt = 0; float total = 0; };

// 设置（挂在 world 实体上）
ecs.set<Time>({0.016f, 0});

// 在系统中读取
ecs.system<Position, Velocity>()
    .each([&](flecs::iter &it, Position &p, Velocity &v) {
        const Time *t = it.world().get<Time>();
        p.x += v.dx * t->dt;
    });

// 修改单例
Time *t = ecs.get_mut<Time>();
t->total += t->dt;
ecs.modified<Time>();
```

---

## 7.8 多线程

Flecs 支持多线程系统执行：

```cpp
// 设置工作线程数
ecs.set_threads(4);

// 系统自动分片（每个线程处理一部分实体）
ecs.system<Position, Velocity>()
    .multi_threaded()  // 标记为可并行
    .each([](Position &p, Velocity &v) {
        p.x += v.dx;  // 每个线程独立处理不同实体，无竞争
    });
```

**注意**：多线程系统内不能增删组件/实体（会导致 archetype 迁移）。

---

## 7.9 定时器（Timer）

```cpp
// 每 2 秒执行一次
flecs::entity timer = ecs.entity("MyTimer")
    .set<flecs::Timer>({.timeout = 2.0f, .active = true});

ecs.system<Position>()
    .kind(flecs::OnUpdate)
    .each([](Position &p) { ... });

// 把系统绑定到 timer（每 2 秒触发一次而非每帧）
// 需要 Timer addon
```

---

## 7.10 常用模式速查

| 需求 | 实现 |
|------|------|
| 系统执行顺序 | Phase + depends_on |
| 暂停某类实体 | disable<Component> |
| 全局配置 | 单例组件 |
| 实体间关系 | Relationship（Pair） |
| 场景层级 | ChildOf |
| 资源自动释放 | OnRemove Observer |
| 自动序列化 | 反射 member + JSON addon |
| 编辑器面板 | 反射 member → 自动生成 UI |
| 动画写入 | 反射 member → 按字段名写入 |

---

## 7.11 完整示例：迷你游戏循环

```cpp
#include <flecs.h>
#include <iostream>

struct Position { float x = 0, y = 0; };
struct Velocity { float dx = 0, dy = 0; };
struct Health { int hp = 100; };
struct Enemy { };  // Tag

int main() {
    flecs::world ecs;

    // 组件注册（带反射）
    ecs.component<Position>()
        .member<float>("x", offsetof(Position, x))
        .member<float>("y", offsetof(Position, y));

    // 创建实体
    ecs.entity("Player")
        .set<Position>({0, 0})
        .set<Velocity>({1, 0})
        .set<Health>({100});

    ecs.entity("Goblin")
        .set<Position>({5, 3})
        .set<Velocity>({-1, 0})
        .set<Health>({30})
        .add<Enemy>();

    // 移动系统
    ecs.system<Position, Velocity>()
        .each([](Position &p, Velocity &v) {
            p.x += v.dx;
            p.y += v.dy;
        });

    // 死亡清理（OnRemove 自动释放）
    ecs.observer<Health>()
        .event(flecs::OnSet)
        .with<Enemy>()
        .each([](flecs::entity e, Health &h) {
            if (h.hp <= 0) {
                std::cout << e.name() << " died!\n";
                e.destruct();
            }
        });

    // 游戏循环
    for (int frame = 0; frame < 10; frame++) {
        ecs.progress(1.0f / 60.0f);
    }

    // 输出玩家位置
    auto player = ecs.entity("Player");
    const Position *p = player.get<Position>();
    std::cout << "Player at: (" << p->x << ", " << p->y << ")\n";

    return 0;
}
```

---

## 课程总结

7 课学完，你已掌握 Flecs 的核心能力：

```
01 基础概念 ──→ 02 组件操作 ──→ 03 系统查询
                                      │
04 关系层级 ←── 05 Observer ←── 06 反射序列化
                                      │
                              07 Pipeline + 高级特性
```

下一步：阅读 [Flecs 官方示例](https://github.com/SanderMertens/flecs/tree/master/examples/cpp)，每个示例对应一个特性点，可以逐个运行验证。
