# Game Engine Editor — 常见问题

## 编译问题

### 1. "无法打开包括文件: flecs_no_addons.h"

**原因**：CMake FLECS_DIR 路径错误。
**解决**：检查 `CMakeLists.txt` 中 `set(FLECS_DIR "${CMAKE_SOURCE_DIR}/../flecs-4.1.6/distr")` 路径是否正确。

### 2. "FLECS_IDTagComponentID_ 未声明的标识符"

**原因**：`ECS_COMPONENT` 宏在构造函数内调用，ID 变量作用域仅限于构造函数。
**解决**：使用文件作用域 `inline ecs_entity_t ecs_id(T)` + 构造函数内 `ECS_COMPONENT_DEFINE`。

### 3. LNK2005: 重复定义的符号 (inline 变量)

**原因**：`ECS_COMPONENT_DECLARE` 在头文件中定义全局变量，被 MOC 文件重复编译。
**解决**：使用 `inline ecs_entity_t ecs_id(Type);` 替代 `ECS_COMPONENT_DECLARE(Type)`。

### 4. C7555: 指定初始值设定项需要 C++20

**原因**：`ecs_query(world, {.terms = {...}})` 使用 C99 指定初始化器。
**解决**：设置 `CMAKE_CXX_STANDARD 20`，并用 `ecs_query_desc_t qd = {}; qd.terms[0].id = ...; ecs_query_init(world, &qd)` 替代。

### 5. C3861: FLECS_IDTID_ 找不到 (ecs_set_ptr 模板)

**原因**：模板函数 `template<typename T> void setComponent(ecs_entity_t e, const T &v)` 中 `ecs_set_ptr` 宏无法展开模板参数 T。
**解决**：使用显式重载而非模板：

```cpp
void setComponent(ecs_entity_t e, const TransformComponent &v) { ecs_set_ptr(world(), e, TransformComponent, &v); }
```

### 6. C3863: 不可指定数组类型 "char[64]"

**原因**：`tg->name = t;` 试图将 QString 赋值给 char 数组。
**解决**：使用 `qstrncpy(tg->name, t.toUtf8().constData(), 64);`

### 7. LNK2019: 无法解析的外部符号 ecs_init/ecs_fini

**原因**：Flecs 仅作为 INTERFACE 库引入（header-only），缺少 .c 实现。
**解决**：改为 STATIC 库：

```cmake
add_library(flecs STATIC "${FLECS_DIR}/flecs_no_addons.c")
```

### 8. LNK2019: qt_metacall/metaObject/staticMetaObject

**原因**：Qt MOC 未运行，Q_OBJECT 宏代码未生成。
**解决**：添加 `set(CMAKE_AUTOMOC ON)` 到 CMakeLists.txt。

### 9. MSB3073 后处理错误

**原因**：CMake 生成的 post-build 事件调用 `pwsh.exe` 失败。
**影响**：无——编译和链接已完成，exe 正常。
**解决**：忽略此警告，或在 CMakeLists.txt 中禁用后处理事件。

---

## 运行时问题

### 10. ASSERT: "context" in qopenglfunctions_3_3_core.cpp

**原因**：在 GL 上下文就绪前调用了 `initializeOpenGLFunctions()`。
**解决**：所有需要 GL 上下文的操作推迟到 `Viewport::initializeGL()` 或之后的 `glReady` 信号中执行。

### 11. ASSERT: "len >= 0" in qstringview.h line 174

**原因**：Flecs 使用 `memcpy` 复制含 `QString` 的组件，导致 QString 内部指针损坏。
**解决**：所有组件中不得使用 `QString`，改用固定大小 `char[N]` 数组。
详细说明见 `ARCHITECTURE.md` 第 6.2 节。

### 12. 双击 exe 显示终端窗口后消失

**原因**：exe 编译为控制台程序（`add_executable` 无 `WIN32` 标志）。
**解决**：改为 `add_executable(game_editor WIN32 ...)` 并链接 `Qt5::WinMain`。

### 13. 窗口显示但画面全黑

**可能原因**：

1. 着色器文件路径错误（检查 `SHADER_DIR` 宏定义）
2. 摄像机位置不当（默认在 `(0,2,8)` 看向原点）
3. 光源设置不正确
4. `MeshFactory::createCube()` 在 GL 上下文就绪前被调用

### 14. 无法保存文件（EBUSY: resource busy or locked）

**原因**：MSBuild 进程持有文件句柄未释放。
**解决**：`taskkill /F /IM MSBuild.exe /T` 终止所有 MSBuild 进程。

### 15. 修改组件属性后视口不更新

**原因**：InspectorPanel 的 `componentChanged` 信号未连接。
**检查**：`main_window.h` 中是否有：

```cpp
connect(m_inspector, &InspectorPanel::componentChanged,
        m_viewport, QOverload<>::of(&Viewport::update));
```

---

## 构建命令

```bash
# 完整清理重建
cd d:\code\.std\opengl\game_engine
rmdir /s /q build
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug --target game_editor -j 12

# 增量构建（仅编译修改过的文件）
cd build
cmake --build . --config Debug --target game_editor -j 12

# 强制重编译（清除 .obj 缓存）
rmdir /s /q build\game_editor.dir
cmake --build . --config Debug --target game_editor -j 12
```

## 环境依赖

| 依赖 | 路径 | 版本 |
|------|------|------|
| Qt5 | `D:/QT/5.14.2/msvc2017_64` | 5.14.2 |
| Flecs | `../flecs-4.1.6/distr/` | 4.1.6 |
| MSVC | VS 2022 | 17.x |
| CMake | — | 3.15+ |
