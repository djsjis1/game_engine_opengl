# 01 · ECS 概念与 Flecs 基础

> 理解 ECS 的核心思想，掌握 Flecs 中 World / Entity / Component 三个基本概念，写出第一个可运行程序。

---

## 1.1 什么是 ECS

ECS = **Entity-Component-System**，一种软件架构模式，核心思想：**组合优于继承**。

### 传统 OOP 的问题

```cpp
class GameObject { vec3 position; };
class Movable : public GameObject { vec3 velocity; };
class Renderable : public GameObject { Mesh mesh; };
// 既能移动又能渲染？→ 菱形继承
class Player : public Movable, public Renderable { /* 二义性！ */ };
```

继承链一深就组合爆炸。想加"可拾取"属性？又要新建子类。

### ECS 的解法

```
Entity = 只是一个 ID（uint64），不含数据和方法
Component = 纯数据 struct，挂在实体上
System = 查询"有某些组件"的实体，批量执行逻辑
```

一个"玩家"是组件的集合：

```
Entity #42
  ├── Position { x, y, z }
  ├── Velocity { dx, dy, dz }
  ├── Mesh { handle }
  ├── Health { hp: 100 }
  └── PlayerInput { }    ← 有这个组件 = 玩家控制的
```

想加新功能？加组件 + 写系统，不动已有代码。

---

## 1.2 为什么用 ECS

| 优势 | 说明 |
|------|------|
| 灵活组合 | 任何实体挂任何组件组合，无继承限制 |
| 解耦 | System 之间不直接通信，通过组件数据间接交互 |
| 性能 | 组件连续存储（archetype），遍历缓存友好 |
| 可扩展 | 加组件/系统不改已有代码（开闭原则） |
| 动态性 | 运行时增删组件（装备系统、Buff、状态机） |

---

## 1.3 Flecs 简介

[Flecs](https://github.com/SanderMertens/flecs)：C 编写的 ECS 框架，提供 C++ 包装。

- 轻量：核心约 1 万行 C，无外部依赖
- 快速：archetype 存储 + 查询缓存
- 功能：关系、层级、反射、JSON、Observer、Pipeline、Timer
- 双 API：底层 C（`ecs_*`），上层 C++ 模板（`flecs::world`）

---

## 1.4 第一个程序

```cpp
#include <flecs.h>
#include <iostream>

// 组件：纯数据 struct
struct Position { float x = 0, y = 0; };
struct Velocity { float dx = 0, dy = 0; };

int main() {
    // 1. 创建世界
    flecs::world ecs;

    // 2. 创建实体 + 挂组件
    flecs::entity player = ecs.entity("Player")
        .set<Position>({10.0f, 20.0f})
        .set<Velocity>({1.0f, 0.0f});

    // 3. 创建系统：每帧遍历有 Position + Velocity 的实体
    ecs.system<Position, Velocity>()
        .each([](Position &p, Velocity &v) {
            p.x += v.dx;
            p.y += v.dy;
        });

    // 4. 运行一帧（触发所有系统）
    ecs.progress();

    // 5. 读取结果
    const Position *p = player.get<Position>();
    std::cout << "Player: (" << p->x << ", " << p->y << ")\n";
    // 输出: Player: (11, 20)

    return 0;
}
```

**编译**（假设 flecs 源码在 thirdparty/）：

```bash
g++ main.cpp thirdparty/flecs.c -I thirdparty/ -std=c++17 -o demo
```

---

## 1.5 三个核心概念详解

### World（世界）

所有数据的容器，通常一个程序一个。

```cpp
flecs::world ecs;          // 创建
ecs.progress();            // 推进一帧（执行所有系统）
ecs.progress(1.0f / 60);  // 带 dt 参数
```

### Entity（实体）

只是 ID。可以带名字（调试用），可以挂任意组件组合。

```cpp
flecs::entity e = ecs.entity();           // 匿名实体
flecs::entity e2 = ecs.entity("Enemy");   // 带名字
e2.id();                                  // 获取 uint64 ID
e2.is_alive();                            // 是否还活着
e2.destruct();                            // 销毁（连带所有组件）
```

### Component（组件）

任何 struct/class 都可以当组件。Flecs 自动处理注册。

```cpp
struct Health { int hp = 100; int maxHp = 100; };

// 挂组件（set = 有则覆盖，无则添加）
e.set<Health>({80, 100});

// 读组件（返回 const 指针，不存在返回 nullptr）
const Health *h = e.get<Health>();

// 改组件（返回可写指针）
Health *h2 = e.get_mut<Health>();
h2->hp -= 10;
e.modified<Health>();  // 通知 Flecs 数据被改了（触发 OnSet）
```

---

## 1.6 Archetype 存储模型（理解性能的关键）

Flecs 内部按"组件组合"分组存储（archetype）：

```
Archetype [Position, Velocity]:
  Entity #1: {pos1, vel1}
  Entity #5: {pos5, vel5}
  Entity #9: {pos9, vel9}    ← 内存连续，遍历时缓存命中率高

Archetype [Position, Velocity, Health]:
  Entity #2: {pos2, vel2, hp2}
  Entity #7: {pos7, vel7, hp7}
```

当你查询 `world.each<Position, Velocity>()` 时，Flecs 只遍历包含这两个组件的 archetype，跳过不相关的实体。**组件组合相同的实体在内存中紧挨着**——这就是 ECS 比 OOP 快的根本原因。

---

## 1.7 核心 API 速查

| 操作 | 代码 |
|------|------|
| 创建世界 | `flecs::world ecs;` |
| 创建实体 | `ecs.entity("name")` |
| 添加/覆盖组件 | `e.set<Comp>({...})` |
| 添加组件（不赋值） | `e.add<Comp>()` |
| 读取组件 | `e.get<Comp>()` → `const Comp*` |
| 可写组件 | `e.get_mut<Comp>()` → `Comp*` |
| 检查有无组件 | `e.has<Comp>()` |
| 移除组件 | `e.remove<Comp>()` |
| 销毁实体 | `e.destruct()` |
| 遍历（快捷） | `ecs.each<Comp>([](Comp &c){...})` |
| 创建系统 | `ecs.system<Comp>().each([](Comp &c){...})` |
| 推进一帧 | `ecs.progress(dt)` |

---

## 下一课

[02 · 组件与实体操作](./02-组件与实体操作.md) — 组件的完整生命周期、实体增删改查、Tag 组件、单例组件。
