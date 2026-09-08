# 02 · 注册 C 函数到 Lua

> 上一篇讲了栈，这一篇讲怎么用栈把 C 函数变成 Lua 可调用的方法。

---

## 2.1 C 函数的签名规范

暴露给 Lua 的 C 函数必须遵循固定签名：

```c
static int my_c_function(lua_State *L) {
    // 从栈读参数（索引 1, 2, 3...）
    // 把返回值压栈
    return N;  // N = 返回值的个数
}
```

**必须是 `static int (*)(lua_State *)` 类型**。

---

## 2.2 三种注册方式

### 方式一：注册到全局表（最简单的入门）

```c
// 定义一个 C 函数
static int api_add(lua_State *L) {
    double a = luaL_checknumber(L, 1);  // 第 1 个参数
    double b = luaL_checknumber(L, 2);  // 第 2 个参数
    lua_pushnumber(L, a + b);           // 压入结果
    return 1;                           // 返回 1 个值
}

// 注册：让 Lua 可以调用 add(3, 4)
lua_pushcfunction(L, api_add);   // 把 C 函数指针压栈
lua_setglobal(L, "add");         // 设为全局变量 _G["add"] = api_add

// Lua 端:
// local result = add(3, 4)  → result = 7
```

**全局注册的缺点**：所有脚本实体都能看到，容易命名冲突。

---

### 方式二：注册到 table 的方法（本项目采用）

```c
// 创建一个 table（作为命名空间/沙盒）
lua_newtable(L);  // 栈: [table]

// 注册多个函数到这个 table
lua_pushcfunction(L, api_getPosition);
lua_setfield(L, -2, "get_position");
//   ↑        ↑      ↑
// 设字段   目标table  字段名
// 效果: table["get_position"] = api_getPosition

lua_pushcfunction(L, api_setPosition);
lua_setfield(L, -2, "set_position");
// 效果: table["set_position"] = api_setPosition
```

**`lua_setfield` 的工作原理**：

```
执行前:                         执行后:
栈: [table, function]    →     栈: [table]
table["key"] = function          function 已从栈中移走
```

### 方式三：直接设到 table 的指定索引（injectApi 写法）

本项目 `injectApi()` 函数的典型模式：

```c
static void injectApi(lua_State *L) {
    // 此时栈上已经有一个 table（栈顶是沙盒 table）
    // L 的栈: [..., sandboxTable]

    lua_pushcfunction(L, api_getPosition);  // [table, func]
    lua_setfield(L, -2, "get_position");    // [table]  ← func 已移走

    lua_pushcfunction(L, api_setPosition);  // [table, func]
    lua_setfield(L, -2, "set_position");    // [table]

    // 重复 15 次，注入 15 个 API 函数
}
```

---

## 2.3 手把手：从零写一个加法器

### 完整的可运行示例

```c
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

// ─── C 函数定义 ───
static int api_add(lua_State *L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);
    return 1;
}

static int api_subtract(lua_State *L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a - b);
    return 1;
}

int main() {
    // 1. 创建 VM
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // 2. 注册函数到全局表
    lua_pushcfunction(L, api_add);
    lua_setglobal(L, "add");

    lua_pushcfunction(L, api_subtract);
    lua_setglobal(L, "subtract");

    // 3. 执行 Lua 代码
    const char *code =
        "local x = add(10, 5)       \n"  // x = 15
        "local y = subtract(x, 3)   \n"  // y = 12
        "print('Result:', y)        \n"; // Result: 12

    if (luaL_dostring(L, code) != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
    }

    lua_close(L);
    return 0;
}
```

### 执行流程可视化

```
luaL_dostring(L, "add(10, 5)");

C++ 端                        Lua VM 内部
───────                       ───────────
                              luaL_dostring 遇到 add(10,5)
                              ↓
                              把 "add" 从 _G 取出 → 是 C 函数指针
                              把 10、5 依次压栈
                              ↓
                              调用 api_add(L)
                              ↓
api_add 开始执行:
  luaL_checknumber(L, 1)  → 10
  luaL_checknumber(L, 2)  → 5
  lua_pushnumber(L, 15)
  return 1                → Lua VM 收到返回值 15
                              ↓
                              把 15 赋给 x
```

---

## 2.4 本项目的 injectApi 全貌

本项目为每个脚本实体的沙盒 table 注入 15 个 API 函数：

```
注入的 API 函数分类:

┌─ 实体自身 Transform 读写（零拷贝，直接操作 C++ 内存）
│   get_position() → x, y, z
│   set_position(x, y, z)
│   get_rotation() → x, y, z
│   set_rotation(x, y, z)
│   get_scale() → x, y, z
│   set_scale(x, y, z)
│
├─ 全局查询 / 创建
│   get_node("名称") → entity_id
│   is_key_pressed("W") → true/false
│   delta() → dt
│   create_entity("名称") → entity_id
│
├─ 跨实体操作
│   get_node_position(entity_id) → x, y, z
│   set_node_position(entity_id, x, y, z)
│
└─ 信号系统
    connect("信号名", callback)
    emit("信号名", 参数...)
    connect_to(entity_id, "信号名", callback)
```

**注册原理都一样**：

```c
lua_pushcfunction(L, api_getPosition);  // 把 C 函数压栈
lua_setfield(L, -2, "get_position");    // 挂到 table 上
```

只是重复了 15 次。

---

## 2.5 Lua 端调用方式

### 冒号语法糖 `self:method()`

```lua
-- Lua 中以下两种写法等价:

self:set_position(0, 0, 0)
-- 等价于
self.set_position(self, 0, 0, 0)
```

`:` 冒号会自动把 `self`（即 table 本身）作为第一个参数传进去。虽然我们的 C 函数不读第一个参数（`self`），但这是 Lua 的惯例写法。

---

## 2.6 常见错误

### 错误 1：忘记 return

```c
static int api_bad(lua_State *L) {
    lua_pushnumber(L, 42);
    // 忘记 return 1;  ← Lua VM 不知道栈上有返回值！
}
// Lua 端收到 nil，不是 42
```

### 错误 2：返回值个数写错

```c
static int api_bad(lua_State *L) {
    lua_pushnumber(L, 10);
    lua_pushnumber(L, 20);
    lua_pushnumber(L, 30);
    return 1;  // ❌ 应该是 3，否则只返回 10
}
```

### 错误 3：栈泄漏

```c
static int api_bad(lua_State *L) {
    lua_getglobal(L, "someFunc");  // 压栈
    // 忘记 lua_pop(L, 1);
    return 0;
}
// 栈上残留了值，后续调用会读到错误的参数！
```

---

## 2.7 总结

```
注册 C 函数到 Lua 三步:

  ① 写一个 static int func(lua_State *L) 函数
     参数从栈读 (luaL_checknumber(L, 1))
     结果压栈 (lua_pushnumber(L, ...))
     返回个数 (return N)

  ② 用 lua_pushcfunction 把函数指针压栈

  ③ 用 lua_setfield / lua_setglobal 挂到目标位置
```

---

**下一篇**: [03-核心-上下文传递模式](./03-核心-上下文传递模式.md) —— 本项目最精妙的设计：`__active_ctx`
