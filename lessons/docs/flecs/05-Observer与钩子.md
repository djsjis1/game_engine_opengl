# 05 · Observer 与钩子

> 组件生命周期事件监听、钩子函数、资源自动释放模式。

---

## 5.1 什么是 Observer

Observer = **事件监听器**。当组件被添加/移除/修改时，自动触发回调。

```cpp
// 监听 Position 组件被添加
ecs.observer<Position>()
    .event(flecs::OnAdd)
    .each([](flecs::entity e, Position &p) {
        std::cout << e.name() << " 获得了 Position\n";
    });
```

---

## 5.2 三种核心事件

| 事件 | 触发时机 | 典型用途 |
|------|---------|---------|
| `OnAdd` | 组件首次添加到实体 | 初始化、注册 |
| `OnRemove` | 组件从实体移除（含实体销毁） | 资源释放、清理 |
| `OnSet` | 组件数据被修改（`set` 或 `modified`） | 脏标记、同步 |

```cpp
// OnAdd：实体获得 Mesh 时分配 GPU 资源
ecs.observer<Mesh>()
    .event(flecs::OnAdd)
    .each([](flecs::entity e, Mesh &m) {
        m.gpuHandle = allocateGpuMesh(m);
    });

// OnRemove：实体失去 Mesh 时释放 GPU 资源
ecs.observer<Mesh>()
    .event(flecs::OnRemove)
    .each([](flecs::entity e, Mesh &m) {
        freeGpuMesh(m.gpuHandle);
    });

// OnSet：Transform 被修改时标记脏
ecs.observer<Transform>()
    .event(flecs::OnSet)
    .each([](flecs::entity e, Transform &t) {
        t.dirty = true;
    });
```

---

## 5.3 资源自动释放模式（核心设计模式）

**问题**：实体可以通过多种方式死亡（手动 destruct、父实体级联删除、移除组件），每条路径都要释放资源？

**解法**：OnRemove Observer 覆盖所有死亡路径。

```cpp
struct Texture {
    uint32_t gpuId = 0;
    int refCount = 0;
};

// 无论实体怎么死，Texture 被移除时自动触发
ecs.observer<Texture>()
    .event(flecs::OnRemove)
    .each([](flecs::entity e, Texture &tex) {
        if (tex.gpuId) {
            glDeleteTextures(1, &tex.gpuId);
            tex.gpuId = 0;
        }
    });

// 之后任何地方都不需要"记得释放"：
e.destruct();           // ✅ 触发 OnRemove
parent.destruct();      // ✅ 级联删除子实体，也触发 OnRemove
e.remove<Texture>();    // ✅ 手动移除，也触发
```

**关键认知**：OnRemove 在组件数据还有效时触发（可以读取组件内容做清理），之后数据才被销毁。

---

## 5.4 钩子函数（Hook）

钩子是组件级别的构造/析构回调（比 Observer 更底层）：

```cpp
ecs.component<Texture>()
    .on_add([](flecs::entity e, Texture &tex) {
        // 组件添加时
    })
    .on_remove([](flecs::entity e, Texture &tex) {
        // 组件移除时
    })
    .on_set([](flecs::entity e, Texture &tex) {
        // 组件数据被设置时
    });
```

**Observer vs Hook**：

- Hook：绑定在组件类型上，全局生效
- Observer：可以加过滤条件（如只监听有某 Tag 的实体）

---

## 5.5 多事件监听

```cpp
// 同时监听 OnAdd 和 OnSet
ecs.observer<Position>()
    .event(flecs::OnAdd)
    .event(flecs::OnSet)
    .each([](flecs::entity e, Position &p) {
        // 添加或修改时都触发
    });
```

---

## 5.6 带过滤的 Observer

```cpp
// 只监听有 Enemy 标记的实体的 Health 变化
ecs.observer<Health>()
    .event(flecs::OnSet)
    .with<Enemy>()  // 过滤条件
    .each([](flecs::entity e, Health &h) {
        if (h.hp <= 0) {
            // 敌人死亡逻辑
        }
    });
```

---

## 5.7 实战模式总结

| 模式 | 实现 |
|------|------|
| GPU 资源自动释放 | OnRemove → glDelete* |
| 引用计数归还 | OnRemove → refCount-- |
| 信号连接清理 | OnRemove → 遍历连接表 unref |
| 脏标记 | OnSet → dirty = true |
| 初始化默认值 | OnAdd → 设置默认状态 |
| 物理体同步 | OnSet(Transform) → 更新物理引擎位置 |

---

## 5.8 注意事项

- Observer 回调中**不要增删组件**（会导致递归触发）
- OnRemove 中组件数据仍有效，可以读取做清理
- 实体销毁时，组件移除顺序不确定（不要依赖顺序）
- Observer 是同步的（在触发操作内部立即执行）

---

## 下一课

[06 · 反射与序列化](./06-反射与序列化.md) — member 注册、EcsStruct 元数据、JSON 自动序列化。
