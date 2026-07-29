# 07 · 项目 LuaApi.cpp 完整走读

> 把前 6 篇的知识串起来，详细解析本项目 `my_demo/System/LuaApi.cpp` 的每一部分。

---

## 7.1 文件结构总览

```
LuaApi.cpp (约 480 行)

  ┌─ [1-33]   文件头部注释 + include
  │
  ├─ [35-46]  全局输入状态 (g_keys, g_deltaTime...)
  │
  ├─ [48-54]  编译缓存 (CachedChunk + g_compiledChunks)
  │
  ├─ [56-71]  上下文传递 (LuaCtx + getCtx)
  │
  ├─ [76-190] Lua API 函数 (15 个暴露给 Lua 的 C 函数)
  │   ├── get/set_position, rotation, scale
  │   ├── get_node, is_key_pressed, delta, create_entity
  │   └── get/set_node_position
  │
  ├─ [192-278] 信号系统 (connect, emit, connect_to)
  │
  ├─ [280-312] injectApi (把 C 函数注入沙盒 table)
  │
  ├─ [314-327] loadSource (读 .lua 文件)
  │
  ├─ [329-382] compileAndLoad (编译 + 字节码缓存)
  │
  ├─ [384-407] initEntity (创建沙盒环境)
  │
  ├─ [409-426] callLifecycle (调用 ready/update)
  │
  └─ [428-480] ScriptSystem 实现 (init/shutdown/update)
```

---

## 7.2 第 1 段：include 与宏保护

```c
// Lua 的头文件为什么用 extern "C" 包起来？
// 因为 Lua 是纯 C 写的，而我们的项目是 C++。
// 如果不加 extern "C"，链接器会用 C++ 的 name mangling 找符号，
// 找不到 Lua 的 C 符号 → 链接错误。

#pragma push_macro("LUA_API")    // 保存可能冲突的宏
#pragma push_macro("LUAI_FUNC")
#pragma push_macro("LUALIB_API")
extern "C" {
#include <lua.h>      // Lua 核心 API
#include <lauxlib.h>   // 辅助库 (luaL_checknumber 等)
#include <lualib.h>    // 标准库 (luaL_openlibs)
}
#pragma pop_macro("LUALIB_API")  // 恢复宏
#pragma pop_macro("LUAI_FUNC")
#pragma pop_macro("LUA_API")
```

---

## 7.3 第 2 段：全局输入状态

```c
// 为什么用全局变量而不是注册表？
// → 注册表适合存"与当前实体相关"的数据
// → 输入状态是"全局唯一"的（同一时刻只有一套键鼠状态）
// → 用静态全局变量更直接

static bool g_keys[256] = {};      // 256 个键的按下状态（按 ASCII 索引）
static float g_mouseDX = 0;        // 本帧鼠标水平位移
static float g_mouseDY = 0;        // 本帧鼠标垂直位移
static float g_deltaTime = 0;      // 上一帧到本帧的间隔（秒）

// extern "C" 保证 MainWindow.cpp 能通过声明找到这些函数
extern "C" void scriptSetInput(const bool keys[256], float dx, float dy) {
    memcpy(g_keys, keys, sizeof(g_keys));  // 拷贝 256 字节
    g_mouseDX = dx;
    g_mouseDY = dy;
}
extern "C" void scriptSetDelta(float dt) { g_deltaTime = dt; }
```

---

## 7.4 第 3 段：编译缓存

```c
// 问题: 每帧都重新编译 .lua 源文件太慢了
// 解法: 编译一次 → lua_dump 导出字节码 → 缓存到 C++ 内存
//       后续帧直接 luaL_loadbuffer 加载字节码

struct CachedChunk {
    std::string data;    // 字节码（二进制，不是文本）
    time_t mtime = 0;    // 文件修改时间（判断是否需要重编译）
};

static std::map<std::string, CachedChunk> g_compiledChunks;
// key = 文件路径（如 "scripts/demo.lua"）

// 缓存逻辑在 compileAndLoad() 中:
//   1. stat(文件路径, &st) → 获取 mtime
//   2. 如果 g_compiledChunks[key].mtime == st.st_mtime → 命中缓存
//   3. 直接 luaL_loadbuffer(L, 字节码, 大小)
//   4. 否则重新编译 → lua_dump 导出 → 更新缓存
```

---

## 7.5 第 4 段：上下文传递

```c
struct LuaCtx {
    TransformComponent *tf;   // 当前实体的 Transform（零拷贝读写！）
    flecs::world *w;          // ECS 世界（查/创其他实体）
    flecs::entity_t id;       // 当前实体的 ID
};

// getCtx 被 15 个 API 函数在入口处调用
static LuaCtx *getCtx(lua_State *L) {
    lua_pushstring(L, "__active_ctx");         // key
    lua_gettable(L, LUA_REGISTRYINDEX);        // registry["__active_ctx"]
    auto *c = (LuaCtx *)lua_touserdata(L, -1); // lightuserdata → 指针
    lua_pop(L, 1);                             // 清理栈
    return c;
}
```

**知识点回顾**（详见 [03-上下文传递模式](./03-核心-上下文传递模式.md)）：

- `lua_pushlightuserdata` 存的是裸指针，不管理生命周期
- 注册表对 Lua 代码不可见，是 C↔C 之间传递数据的通道
- ctx 在 C++ 栈上，只在单次 update() 调用期间有效

---

## 7.6 第 5 段：15 个 Lua API 函数

### Transform 读写（零拷贝）

```c
// 读: 从 c->tf 指针直接读 C++ 内存
static int api_getPosition(lua_State *L) {
    auto *c = getCtx(L);
    lua_pushnumber(L, c->tf->position.x);  // 零拷贝读
    lua_pushnumber(L, c->tf->position.y);
    lua_pushnumber(L, c->tf->position.z);
    return 3;  // 3 个返回值
}

// 写: 直接写 c->tf 指向的 C++ 内存
static int api_setPosition(lua_State *L) {
    auto *c = getCtx(L);
    c->tf->position.x = (float)luaL_checknumber(L, 1);  // 零拷贝写！
    c->tf->position.y = (float)luaL_checknumber(L, 2);
    c->tf->position.z = (float)luaL_checknumber(L, 3);
    return 0;
}
```

同理：`get/set_rotation`、`get/set_scale`。

### 全局查询

```c
// get_node("名字") → entity_id
// 遍历所有有 TagComponent 的实体，匹配名字
static int api_getNode(lua_State *L) {
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    c->w->each([&](flecs::entity e, const TagComponent &tag) {
        if (strcmp(tag.name, name) == 0)
            lua_pushinteger(L, (lua_Integer)e.raw_id());
    });
    return lua_gettop(L) > 1 ? 1 : 0;  // 找到返回 1，没找到返回 0
}

// is_key_pressed("W") → true/false
// 小写自动转大写（统一索引）
static int api_isKeyPressed(lua_State *L) {
    const char *k = luaL_checkstring(L, 1);
    char c = (k[0] >= 'a' && k[0] <= 'z') ? k[0] - 32 : k[0];
    lua_pushboolean(L, g_keys[(unsigned char)c]);
    return 1;
}
```

### 跨实体操作

```c
// get_node_position(targetId) → x, y, z
// 读取任意实体的位置（不限于当前脚本实体）
static int api_getNodePosition(lua_State *L) {
    auto *c = getCtx(L);
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    // 用 C API 查询（Flecs C++ e.get<T>() 在 MSVC 有 bug）
    auto *tf = (TransformComponent *)ecs_get_id(
        c->w->c_ptr(), eid, c->w->component<TransformComponent>().raw_id());
    if (!tf) {
        lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
        return 3;
    }
    lua_pushnumber(L, tf->position.x);
    lua_pushnumber(L, tf->position.y);
    lua_pushnumber(L, tf->position.z);
    return 3;
}
```

**为什么用 `ecs_get_id` 而不是 `e.get<TransformComponent>()`？**
→ Flecs 4.1.6 的 `entity::get<T>()` 在 MSVC 有模板实例化 bug，用 C API 绕过。

---

## 7.7 第 6 段：信号系统

详见 [06-实战-信号系统设计](./06-实战-信号系统设计.md)。

核心流程回顾：

```
connect("hit", callback):
  ① luaL_ref 存回调 → 得整数 ref
  ② 回调也存到 _G["c" + ref]
  ③ 写入 signalMap["hit"] = [{entityId, "c" + ref}]

emit("hit", args...):
  ① 查 signalMap["hit"] → 遍历监听者列表
  ② lua_getglobal(L, "c3") → 取回调函数
  ③ lua_pcall 调用，传入 args

connect_to(targetId, "hit", callback):
  ① 在目标实体的 signalMap 注册（同 connect）
  ② ★ 在本实体的 listensTo 记录反向索引
```

---

## 7.8 第 7 段：injectApi

```c
// 给每个沙盒 table 注入 15 个 API 函数
static void injectApi(lua_State *L) {
    // 此时栈顶是沙盒 table
    
    // 模式: 重复 15 次
    lua_pushcfunction(L, api_getPosition);  // 压函数
    lua_setfield(L, -2, "get_position");    // table["get_position"] = 函数
    
    // ... 15 次 ...
}
```

**知识点回顾**（详见 [02-注册C函数到Lua](./02-基础-注册C函数到Lua.md)）：

- `lua_pushcfunction` 把 C 函数指针压栈
- `lua_setfield(L, -2, "name")` 把栈顶的函数设到 table 的 "name" 字段
- `-2` 表示目标 table 在当前栈顶的下面

---

## 7.9 第 8 段：脚本加载与编译

```c
// loadSource: 读文件或取内联代码
static std::string loadSource(const ScriptComponent &sc) {
    if (sc.filePath[0]) {
        std::ifstream f(sc.filePath);  // 读文件
        if (f.is_open()) {
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return sc.source[0] ? std::string(sc.source) : "";
}

// compileAndLoad: 编译 + 缓存
//   首次 → luaL_loadstring 编译 → lua_dump 导出字节码 → 缓存
//   后续 → luaL_loadbuffer 加载字节码（跳过编译）
//   文件修改 → mtime 变化 → 重新编译
```

---

## 7.10 第 9 段：initEntity —— 最核心的函数

```c
static bool initEntity(lua_State *L, const ScriptComponent &sc, int &outEnvRef) {
    // ===== ① 创建沙盒 table =====
    lua_newtable(L);                    // 栈: [sandbox]
    
    // ===== ② 设置元表 =====
    lua_newtable(L);                    // 栈: [sandbox, mt]
    lua_getglobal(L, "_G");            // 栈: [sandbox, mt, _G]
    lua_setfield(L, -2, "__index");    // mt.__index = _G
    lua_setmetatable(L, -2);           // sandbox 的元表 = mt
    
    // ===== ③ 注入 API =====
    injectApi(L);  // 给 sandbox 加 15 个方法
    
    // ===== ④ 保存沙盒 =====
    outEnvRef = luaL_ref(L, LUA_REGISTRYINDEX);
    
    // ===== ⑤ 编译脚本 =====
    int funcRef;
    if (!compileAndLoad(L, sc, &funcRef)) return false;
    
    // ===== ⑥ 替换 _ENV =====
    lua_rawgeti(L, LUA_REGISTRYINDEX, funcRef);  // 取出编译好的函数
    lua_rawgeti(L, LUA_REGISTRYINDEX, outEnvRef); // 取出沙盒 table
    lua_setupvalue(L, -2, 1);  // ★ 函数._ENV = sandbox
    
    // ===== ⑦ 执行脚本（定义 ready/update 函数）=====
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[Lua] init: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    luaL_unref(L, LUA_REGISTRYINDEX, funcRef);  // 释放编译产物的引用
    return true;
}
```

**知识点回顾**（详见 [04-沙盒环境与元表](./04-核心-沙盒环境与元表.md)）：

- `__index = _G` 让沙盒能访问 print、math 等全局函数
- `lua_setupvalue` 替换 `_ENV`，让脚本变量写入沙盒而非 _G

---

## 7.11 第 10 段：callLifecycle

```c
static void callLifecycle(lua_State *L, int envRef, const char *name, float dt) {
    // ① 取出沙盒 table
    lua_rawgeti(L, LUA_REGISTRYINDEX, envRef);
    
    // ② 取 table[name]（如 table["update"]）
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);  // 不是函数 → 清理栈
        return;
    }
    
    // ③ 调用
    if (dt > 0) lua_pushnumber(L, dt);  // update 传 dt 参数
    if (lua_pcall(L, dt > 0 ? 1 : 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[Lua] %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出沙盒 table
}
```

---

## 7.12 第 11 段：ScriptSystem 实现

```c
struct ScriptSystem::Impl {
    lua_State *L = nullptr;                     // 全局唯一的 Lua VM
    std::map<flecs::entity_t, int> envRefs;     // 实体 → 沙盒的注册表引用
    std::map<flecs::entity_t, bool> wasReady;   // 是否已调过 ready()
};

void ScriptSystem::init() {
    m_impl = new Impl;
    m_impl->L = luaL_newstate();    // 创建 VM
    luaL_openlibs(m_impl->L);       // 加载标准库
}

void ScriptSystem::shutdown() {
    if (m_impl) {
        if (m_impl->L) lua_close(m_impl->L);  // 销毁 VM
        delete m_impl;
        m_impl = nullptr;
    }
}

void ScriptSystem::update(flecs::world &world, float dt) {
    if (!m_enabled || !m_impl || !m_impl->L) return;
    scriptSetDelta(dt);  // 注入帧间隔
    lua_State *L = m_impl->L;
    
    world.each([&](flecs::entity e, TransformComponent &tf, ScriptComponent &sc) {
        // ① 首次遇到 → 初始化沙盒
        auto it = m_impl->envRefs.find(e.raw_id());
        if (it == m_impl->envRefs.end()) {
            int envRef;
            if (!initEntity(L, sc, envRef)) return;
            m_impl->envRefs[e.raw_id()] = envRef;
            m_impl->wasReady[e.raw_id()] = false;
            it = m_impl->envRefs.find(e.raw_id());
        }
        
        // ② 设置上下文
        LuaCtx ctx = { &tf, &world, e.raw_id() };
        lua_pushstring(L, "__active_ctx");
        lua_pushlightuserdata(L, &ctx);
        lua_settable(L, LUA_REGISTRYINDEX);
        
        // ③ 调用生命周期
        if (!m_impl->wasReady[e.raw_id()]) {
            callLifecycle(L, it->second, "ready");
            m_impl->wasReady[e.raw_id()] = true;
        }
        callLifecycle(L, it->second, "update", dt);
    });
}
```

---

## 7.13 完整调用链路图

```
MainWindow::游戏循环 (每 16ms)
│
├── scriptSetInput(keys, dx, dy)  → 注入到 g_keys, g_mouseDX/DY
│
└── Engine::runFrame(dt)
      │
      └── ScriptSystem::update(world, dt)
            │
            ├── scriptSetDelta(dt)  → 注入到 g_deltaTime
            │
            │  遍历 (Transform + ScriptComponent) 实体:
            │
            ├── 首次? → initEntity()
            │   ├── lua_newtable           → 创建沙盒
            │   ├── {__index = _G}         → 元表
            │   ├── injectApi              → 注入 15 个函数
            │   ├── luaL_ref               → 保存到注册表
            │   ├── compileAndLoad         → 编译脚本（或加载缓存）
            │   └── lua_pcall              → 执行脚本（定义 ready/update）
            │
            ├── LuaCtx {&tf, &world, id} → 写入 __active_ctx
            │
            ├── 首次? → callLifecycle("ready")
            │
            └── callLifecycle("update", dt)
                  │
                  ├── lua_rawgeti → 取出沙盒 table
                  ├── lua_getfield → table["update"]
                  └── lua_pcall(..., dt) → 执行 update(dt)
                        │
                        └── 脚本里调用 self:set_position(...)
                              │
                              └── api_setPosition(L)
                                    │
                                    ├── getCtx(L) → 拿到 ctx
                                    └── ctx->tf->position = (x, y, z)
                                          ↑
                                     零拷贝！直接写了 C++ 内存
```

---

## 7.14 总结

```
学完这 7 篇文档，你应该理解:

  ✅ Lua 的栈是 C↔Lua 通信的唯一通道
  ✅ 如何注册 C 函数给 Lua 调用
  ✅ __active_ctx 上下文传递模式（零拷贝！）
  ✅ 沙盒 table + 元表实现多实体隔离
  ✅ luaL_ref 引用系统防止 GC 回收
  ✅ connect/emit 信号系统的双向索引
  ✅ 完整的 ScriptSystem 每帧执行流程
```

---

**返回**: [01-基础-虚拟机与栈](./01-基础-虚拟机与栈.md)
