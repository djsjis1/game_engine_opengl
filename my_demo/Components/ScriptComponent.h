#ifndef SCRIPTCOMPONENT_H
#define SCRIPTCOMPONENT_H

// ============================================================
// ScriptComponent — Lua 脚本组件（Godot 风格）
//
// filePath: .lua 文件路径（多个实体可共享同一文件）
// source:   内联 Lua 代码（filePath 为空时使用）
//           可通过编辑栏直接修改
// ============================================================
struct ScriptComponent
{
    char filePath[256] = {}; // 脚本文件路径
    char source[1024] = {};  // 内联代码（filePath 优先）
    bool wasReady = false;// 是否已调用过 ready() 方法
};

#endif
