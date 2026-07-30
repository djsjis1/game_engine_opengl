#ifndef ANIMATIONCOMPONENT_H
#define ANIMATIONCOMPONENT_H

// ============================================================
// AnimationComponent — 属性动画播放器（对标 Godot AnimationPlayer）
//
// 【概念对照】
//   Godot AnimationPlayer 节点  →  本组件（播放状态）
//   Godot Animation 资源(.tres) →  .anim 文件（JSON，可含多个命名 clip）
//   Godot play("walk")          →  Lua: anim_play("walk")
//
// 【.anim 文件格式】
//   {
//     "clips": {
//       "spin":   { "duration": 2.0,
//                   "tracks": [ { "target": "rotation",
//                                 "keys": [ {"t":0.0,"v":[0,0,0]},
//                                           {"t":2.0,"v":[0,360,0]} ] } ] },
//       "bounce": { ... }
//     }
//   }
//   target: "position" | "rotation" | "scale"（每帧采样插值写入 Transform）
//
// 【字段说明】
//   animFile : .anim 文件路径（AnimationSystem 按路径缓存解析结果）
//   current  : 正在播放的 clip 名
//   playing  : 是否播放中（编辑器勾选即可预览，对标 Godot 编辑器预览）
//   autoplay : 进入运行模式（Play）时自动从头播放 current 动画
//              （对标 Godot AnimationPlayer.autoplay，默认开：点运行即见动画）
//   loop     : 循环播放（到尾部回到开头）
//   speed    : 播放速度倍率（1=正常，2=两倍速，负数=倒放）
//   time     : 当前播放位置（秒，运行时状态）
//
// 注意：本组件含 char 数组，不注册 Flecs 反射，
// 编辑器（特殊面板）与序列化（手工路径）单独处理——与 ScriptComponent 同模式
// ============================================================
struct AnimationComponent
{
    char animFile[256] = {}; // .anim 文件路径
    char current[64] = {};   // 当前 clip 名
    bool playing = false;    // 是否播放中
    bool autoplay = true;    // ★ 点"运行"自动开播（Godot autoplay 同款）
    bool loop = true;        // 是否循环播放
    float speed = 1.0f;      // 播放速度倍率（1=正常，2=两倍速，负数=倒放）
    float time = 0.0f;       // 当前播放位置（秒，运行时状态）

    // ---- 动画混合/过渡（对标 Godot AnimationTree 的 crossfade）----
    // 从 prev 动画平滑混合到 current：两个同时采样，权重按 blendTime 爬升
    //   blendDuration<=0 表示无过渡（硬切，与以前行为一致）
    char prev[64] = {};         // 正在淡出的旧 clip 名（空=无过渡）
    float prevTime = 0.0f;      // 旧动画的播放位置（过渡期继续推进）
    float blendTime = 0.0f;     // 过渡已进行时长（秒）
    float blendDuration = 0.0f; // 过渡总时长（0=无过渡）
};

#endif
