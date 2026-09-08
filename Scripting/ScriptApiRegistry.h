#ifndef SCRIPTAPIREGISTRY_H
#define SCRIPTAPIREGISTRY_H

#include <QString>
#include <vector>

struct lua_State; // 前置声明：不拖 lua.h 进高亮器/Qt 文件（避免头污染）

// ============================================================
// ScriptApiRegistry —— Lua 脚本 API 的单一事实来源
//
// 【为什么集中一处】引擎注入的 Lua 函数（set_position/anim_play...）
//   有四个消费者，以前散落各处：
//     ① injectApi     把 C 函数注入 Lua 沙盒（真正生效）
//     ② LuaHighlighter 高亮词表
//     ③ 内置编辑器补全（QCompleter 词库）
//     ④ VSCode 用的 engine_api.lua 存根（含参数/文档）
//   现在合并成一张主表（含函数指针+元数据）：加 API 只需在
//   LuaApi.cpp 的 kApiTable 里加一行，四处全部自动同步。
//   （漏填函数指针直接编译错——比生成器的“静默失效”更安全）
//
// 【分层】函数指针只在 LuaApi.cpp 内可见（api_xxx 是 static），
//   所以主表和 scriptApiList() 的实现都在 LuaApi.cpp；
//   本头文件只留结构体 + 声明，供高亮器/存根/编辑器跨文件读元数据。
//
// 【每条 API 的信息】
//   func    注入用的 C 函数指针（nullptr = 非注入型，如 Lua 内置 print）
//   name    补全/高亮/注册用的函数名
//   params  参数名列表（生成 function name(a,b) 存根用）
//   ret     返回值说明（可空，生成 @return 注解）
//   doc     一句话文档
// ============================================================
struct ScriptApiEntry
{
    int (*func)(lua_State *); // 注入函数指针（=lua_CFunction，不拖 lua.h）
    const char *name;
    const char *params; // 形参，如 "x, y, z"；无参为 ""
    const char *ret;    // 返回说明，如 "number x, number y, number z"；无返回为 ""
    const char *doc;    // 一句话文档
};

// 全部注入的 Lua API（实现在 LuaApi.cpp，与 injectApi 同一张表）
const std::vector<ScriptApiEntry> &scriptApiList();

#endif
