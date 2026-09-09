# Lua 调用 C++ 完整教学

> 基于本项目 `Scripting/LuaApi.cpp` 的实战讲解，从零理解 Lua↔C++ 交互。

---

## 📚 阅读顺序（推荐）

| 序号 | 文档 | 核心内容 | 预计时间 |
|------|------|----------|----------|
| **01** | [虚拟机与栈](./01-基础-虚拟机与栈.md) | lua_State、栈的 push/pop、C 函数签名 | 15 分钟 |
| **02** | [注册 C 函数到 Lua](./02-基础-注册C函数到Lua.md) | lua_pushcfunction、lua_setfield、injectApi | 10 分钟 |
| **03** | [上下文传递模式](./03-核心-上下文传递模式.md) | `__active_ctx`、零拷贝、lightuserdata | 15 分钟 |
| **04** | [沙盒环境与元表](./04-核心-沙盒环境与元表.md) | 多实体隔离、`__index`、`_ENV` 替换 | 15 分钟 |
| **05** | [注册表与引用系统](./05-深入-注册表与引用系统.md) | `luaL_ref`、GC 防护、字节码缓存 | 10 分钟 |
| **06** | [信号系统设计](./06-实战-信号系统设计.md) | connect/emit/connect_to、双向索引 | 15 分钟 |
| **07** | [项目 LuaApi 逐行解读](./07-实战-项目LuaApi逐行解读.md) | 完整走读 480 行代码 | 30 分钟 |

---

## 🎯 学完后你能做什么

- 理解本项目脚本系统的每一行代码
- 自己给引擎添加新的 Lua API（如 `self:shoot()`、`self:play_sound()`）
- 看懂 Godot、Unity 等引擎的脚本绑定原理
- 在任何 C++ 项目中嵌入 Lua 脚本

---

## 🔗 相关项目文件

- [Scripting/LuaApi.cpp](../../../Scripting/LuaApi.cpp) — 本教程的核心讲解对象
- [Systems/ScriptSystem.h](../../../Systems/ScriptSystem.h) — 脚本系统接口
- [Components/SignalComponent.h](../../../Components/SignalComponent.h) — 信号组件数据结构
- [assets/scripts/demo.lua](../../../assets/scripts/demo.lua) — 示例脚本
