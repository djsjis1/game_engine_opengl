#ifndef SPRITECOMPONENT_H
#define SPRITECOMPONENT_H

#include "Components/Reflect.h"
#include <cstdint>

// ============================================================
// SpriteComponent — 精灵帧动画（图片序列，对标 Godot AnimatedSprite）
//
// 【用法】
//   folder = 存放帧图片的目录（绝对路径），按文件名排序作为帧序
//            如 D:/.../sprites/walk 下放 01.png 02.png 03.png...
//   frame  = 当前显示第几帧（0 起）
//
// 【帧动画怎么播？—— 正交组合，零新代码】
//   frame 是 REFLECT int 字段 → 现有动画系统的 Property Track 直接驱动：
//     轨道 "SpriteComponent/frame"：0s→0, 0.1s→1, 0.2s→2 ...
//   编辑器打帧/Lua anim_add_key/缓动/事件轨道全部自动可用。
//   动画系统不认识"图片"，资源系统不认识"动画"——组件字段是解耦点。
//
// 【资源管理】spriteHandle 由 ResourceManager 管理（句柄+引用计数+
//   延迟加载+OnRemove 自动释放）——与 MeshComponent 完全同模式
// ============================================================
struct SpriteComponent
{
    char folder[256] = {}; // 帧图片目录（绝对路径；编辑器填完回车加载）
    REFLECT int frame = 0; // 当前帧（动画轨道 "SpriteComponent/frame" 驱动）

    // 运行时字段（不序列化、反射不可见）
    uint32_t spriteHandle = 0; // ResourceManager 精灵帧集句柄（0=未加载）
};

#endif
