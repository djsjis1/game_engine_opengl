# EditorPanel 学习文档

> 这是项目中最复杂的类，理解了它，就理解了 ECS + Qt 编辑器的核心设计。

---

## 一、它是什么

一个 **Godot 风格的三个面板**，左边 360px 宽，固定在主窗口左侧：

```
┌──────────────┐
│   实体        │  ← QGroupBox > QListWidget
│  [新建][删除] │     显示所有实体（TagComponent.name）
│              │
│  Cube        │  ← 点击选中 → 右侧渲染视口高亮该实体
│  Light       │
│  Camera      │
├──────────────┤
│   组件        │  ← QGroupBox > [下拉+/-] > QListWidget
│  [+][-]      │     显示选中实体拥有的组件
│              │
│  Transform   │  ← 点击切换到该组件的属性编辑器
│  Mesh        │
│  Material    │
├──────────────┤
│   属性        │  ← QGroupBox > QScrollArea
│              │     动态生成编辑器（根据组件反射信息）
│  位置 x[0] y[0] z[0] │
│  旋转 x[0] y[0] z[0] │
│  缩放 x[1] y[1] z[1] │
└──────────────┘
```

**三个面板独立**，单击实体 → 刷新组件列表 → 单击组件 → 刷新属性编辑器。每个面板都是 `QListWidget` 或 `QScrollArea`，通过 Qt 信号槽联动。

---

## 二、核心设计：反射驱动

### 2.1 传统方式（已废弃）

```cpp
// 每加一个组件，都要手写 if-else
if (comp == "Transform") {
    auto *tf = get<TransformComponent>(e);
    drawVec3("位置", tf->position);
    drawVec3("旋转", tf->rotation);
}
if (comp == "Material") {
    auto *mat = get<MaterialComponent>(e);
    drawColor("环境光", mat->ambient);
    // ... 100 行重复代码
}
```

### 2.2 反射方式（当前实现）

**Engine.h 注册一次**：

```cpp
m_world.component<TransformComponent>()
    .member<glm::vec3>("position", offsetof(TransformComponent, position))
    .member<glm::vec3>("rotation", offsetof(TransformComponent, rotation))
    .member<glm::vec3>("scale",    offsetof(TransformComponent, scale));
```

**EditorPanel 自动生成 UI**（不需要改代码）：

```cpp
// 遍历 Flecs 反射存储的成员信息
for (int mi = 0; mi < memberCount; ++mi) {
    const ecs_member_t &m = members[mi];
    // m.name = "position", m.offset = 0, m.type = vec3 的 ID
    
    if (m.type == vec3)      → 创建三个 QDoubleSpinBox
    if (m.type == float)     → 创建一个 QDoubleSpinBox
    if (m.type == bool)      → 创建一个 QCheckBox
}
```

**新增组件只需两处**：

1. `Engine.h` 构造函数：`m_world.component<NewComponent>()`
2. `Engine.h` 的 `registerReflection()` 函数：`.member<float>("x", offsetof(...))`

**EditorPanel 一行不改**。运行时自动发现。

---

## 三、关键数据结构

### 3.1 `m_componentMap` — 组件名到 ID 的映射

```cpp
QMap<QString, flecs::entity_t> m_componentMap;
// "Transform"        → 256
// "Material"         → 257
// "RotateBehavior"   → 258
```

来源：`rebuildComponentList()` 查询所有注册了 `EcsStruct` 反射信息的组件实体。

用途：

- 添加组件的下拉列表（`m_addCompBox`）
- 通过名字找到 `flecs::entity_t`，从而调用 `e.add(compId)` / `e.remove(compId)` 等

### 3.2 `m_propWidgets` — 动态 UI 控件注册表

```cpp
QMap<QString, QWidget*> m_propWidgets;
// "Transform|position0" → QDoubleSpinBox* (位置 X)
// "Transform|position1" → QDoubleSpinBox* (位置 Y)
// "Transform|position2" → QDoubleSpinBox* (位置 Z)
// "Material|shininess"  → QDoubleSpinBox* (高光)
// "CameraComponent|isEditor" → QCheckBox*
```

key 格式：`"组件名|成员名|子成员索引"`（vec3 需要索引 0/1/2）

用途：

- `onPropertyChanged()`：从 UI 控件读值 → 通过 `m.offset` 写入组件内存
- `blockPropertySignals()`：统一控制所有 spinbox/checkbox 的信号阻塞

### 3.3 `ecs_member_t` — Flecs 反射存储的成员元数据

```cpp
struct ecs_member_t {
    const char *name;      // "position", "rotation", "shininess"
    ecs_entity_t type;     // float 的 ID 或 vec3 的 ID
    ecs_size_t offset;     // 在 struct 中的字节偏移
    ecs_size_t size;       // 成员大小（float=4, vec3=12）
};
```

来源：`registerReflection()` 注册时 Flecs 自动填充。

用法：`((float*)((char*)data + m.offset))[vi]` — 通过偏移量直接读写内存。

---

## 四、完整数据流（从点击到画面更新）

### 4.1 点击"新建实体"

```
用户点击 [新建实体]
  ↓
onNewEntity()
  → m_world->entity()                          // 创建实体
  → e.set<TagComponent>({"Entity"})            // 名称
  → e.set<TransformComponent>({})              // 默认 Transform
  ↓
refreshEntityList()
  → m_world->each(TagComponent)                // 遍历所有实体
  → 填充 m_entityList                          // 更新 UI
```

### 4.2 给实体添加 RotateBehavior

```
用户选中实体 → 下拉选 "RotateBehavior" → 点击 [+]
  ↓
onAddComponent()
  → m_componentMap["RotateBehavior"] → compId
  → 读取反射：EcsStruct 成员 = [speedX, speedY, speedZ]（三个 float）
  → 计算总大小 = 12 字节
  → new char[12]()                               // 分配清零内存
  → e.set_ptr(compId, 12, data)                  // 设置组件
  ↓
refreshComponentList()
  → ecs_get_type(e.raw_id())                     // 获取实体类型
  → 遍历 type->array[i]                          // 列出所有组件 ID
  → 过滤后填入 m_compList
  ↓
emit valueChanged() → MainWindow → renderWidget->update() → 重绘
```

### 4.3 修改旋转速度

```
用户拖动 speedY 滑块到 90
  ↓
onPropertyChanged()
  → m_componentMap["RotateBehavior"] → compId
  → 读取 EcsStruct 反射信息
  → e.get_mut(compId) → void* data               // 获取组件数据指针
  → 遍历成员：
      m.name = "speedY", m.offset = 4
      spin = m_propWidgets["RotateBehavior|speedY"]
      *(float*)((char*)data + 4) = spin->value()  // = 90.0f
  → ecs_modified_id(...)                          // 通知 Flecs 数据已变
  ↓
emit valueChanged() → 重绘
  ↓
下一帧 RotationSystem::update()
  → tf.rotation.y += 90.0f * 0.016 = 1.44°
  → paintGL() 渲染，立方体开始旋转
```

---

## 五、关键方法速查

| 方法 | 触发时机 | 做的事 |
|------|---------|--------|
| `setWorld(world)` | MainWindow 构造时调用一次 | 保存 world 指针 + 重建组件列表 |
| `rebuildComponentList()` | setWorld 时 | 查询所有反射组件，填充下拉列表 |
| `onNewEntity()` | 点击[新建实体] | 创建实体 + TagComponent + TransformComponent |
| `onDeleteEntity()` | 点击[删除] | `e.destruct()` 销毁实体 |
| `onAddComponent()` | 下拉选组件 + 点击[+] | 反射驱动添加组件 |
| `onRemoveComponent()` | 选中组件 + 点击[-] | `e.remove(compId)` |
| `onEntitySelected()` | 点击实体列表项 | 刷新组件列表 + 属性编辑器 |
| `onComponentSelected()` | 点击组件列表项 | 刷新属性编辑器 |
| `onPropertyChanged()` | 拖动滑块 / 勾选复选框 | 反射写入组件数据 |
| `refreshEntityList()` | 实体增减后 | 遍历 TagComponent 刷新实体列表 |
| `refreshComponentList()` | 选中实体变化后 | 遍历实体类型刷新组件列表 |
| `refreshPropertyEditor()` | 选中组件变化后 | 调用 `createEditorForComponent` 生成动态 UI |
| `createEditorForComponent()` | 属性编辑器刷新时 | 反射驱动：遍历成员 → 创建 spinbox/checkbox |
| `blockPropertySignals(b)` | 刷新属性时 | 临时阻断所有 UI 控件的值变化信号（防循环触发） |

---

## 六、为什么保留三处 C API

| 用途 | API | 原因 |
|------|-----|------|
| 查询所有反射组件 | `ecs_query_init/iter/fini` | C++ query_builder 不支持 EcsStruct 内部元数据类型 |
| 遍历反射成员数组 | `ecs_vec_count/ecs_vec_first` | 成员存储在 `ecs_vec_t`（Flecs 内部容器），不是 `std::vector` |
| 通知数据变更 | `ecs_modified_id` | Flecs 无 entity_t 版本的非模板 `modified()` |

其余全部使用 C++ API：`e.add(id_t)`, `e.remove(id_t)`, `e.get(id_t)`, `e.get_mut(id_t)`, `e.set_ptr(id, size, data)`，这些都是 Flecs 提供的 **非模板、运行时动态** 接口。

---

## 七、Qt 信号槽要点

### 7.1 `valueChanged()` 信号

任何数据修改（新建、删除、加组件、改属性）都 emit 这个信号。MainWindow 连接：

```cpp
connect(m_editor, &EditorPanel::valueChanged,
        m_renderWidget, QOverload<>::of(&QWidget::update));
```

**效果**：编辑栏任何改动 → 立即刷新渲染。

### 7.2 `blockPropertySignals()`

**为什么需要？**

```
1. 用户选中 "RotateBehavior" 组件
2. refreshPropertyEditor() 调 createEditorForComponent()
3. 创建 spinbox，setValue(0) → 触发 valueChanged 信号
4. 信号连接到 onPropertyChanged() → 尝试写入组件
5. 但此时 UI 还在重建中，可能导致崩溃或循环刷新
```

**解法**：

```cpp
blockPropertySignals(true);
QWidget *editor = createEditorForComponent(compId, e);
m_propScroll->setWidget(editor);
blockPropertySignals(false);
```

先阻塞所有信号 → 创建 UI → 设置初始值 → 再恢复信号。这样初始化不会触发属性修改。

### 7.3 `#ifndef Q_MOC_RUN`

```cpp
#ifndef Q_MOC_RUN
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")
#endif
```

Qt 的 `emit` 宏和 Flecs 源码里的 `emit` 变量名冲突。`Q_MOC_RUN` 只在 Qt 的 MOC 预处理器运行时定义，所以这里只在非 MOC 期间包含 flecs.h。MOC 不需要看到 flecs.h。

---

## 八、如何扩展

### 添加新组件支持（只需要改 Engine.h）

1. 创建 `Components/MyComponent.h`
2. 在 `Engine.h` 构造函数加 `m_world.component<MyComponent>()`
3. 在 `registerReflection()` 加成员信息：

   ```cpp
   m_world.component<MyComponent>()
       .member<float>("health", offsetof(MyComponent, health))
       .member<bool>("alive",   offsetof(MyComponent, alive));
   ```

4. **EditorPanel 自动识别**，下拉列表会出现 "MyComponent"

### 添加新的属性类型支持（需要改 EditorPanel）

当前支持：`float`, `glm::vec3`, `bool`。

要加 `int` 类型：

1. 在 `createEditorForComponent` 加 `isInt` 判断
2. 创建 `QSpinBox` 替代 `QDoubleSpinBox`
3. 在 `onPropertyChanged` 加 `int` 写入逻辑

### 添加"运行"按钮

可以在 EditorPanel 加一个 `▶ 运行` 按钮，emit 一个 `runRequested()` 信号给 MainWindow，MainWindow 收到后隐藏编辑栏、全屏渲染。

---

## 九、常见问题

**Q: 为什么 vec3 用三个独立 spinbox 而不是一个？**

A: Qt 没有内置 vec3 输入控件。三个独立 spinbox 对用户更直观（x / y / z）。

**Q: 为什么不支持嵌套结构？**

A: 当前不处理。但如果注册了 `struct Inner { int a; int b; }` 作为 `.member<Inner>(...)`，需要递归遍历 Inner 的成员。这是可行的扩展。

**Q: `m_propWidgets` 里的 key 怎么对应到组件数据？**

A: key = `"组件名|成员名|索引"`。写入时，先用 `e.get_mut(compId)` 拿到组件数据的 `void*` 指针，再用 `m.offset` 偏移到对应字段的地址，直接 cast 为 `float*` 或 `bool*` 写入。
