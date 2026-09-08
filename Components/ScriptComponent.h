#ifndef SCRIPTCOMPONENT_H
#define SCRIPTCOMPONENT_H

#include <map>
#include <string>

// ============================================================
// ScriptComponent — Lua 脚本组件（Godot 风格）
//
// filePath:   .lua 文件路径（多个实体可共享同一文件）
// source:     内联 Lua 代码（filePath 为空时使用）
// properties: @export 变量（key→value，编辑器可见可编辑）
// ============================================================
struct ScriptComponent
{
    char filePath[256] = {};
    char source[1024] = {};
    std::map<std::string, float> properties;
};

#endif
