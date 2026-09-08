#ifndef AUDIOCOMPONENT_H
#define AUDIOCOMPONENT_H

// ============================================================
// AudioComponent — 挂在实体上的音源（对标 Godot AudioStreamPlayer3D）
//
// 【两种用法】
//   实体音源（本组件）：声音跟随实体位置（脚步声、引擎轰鸣、NPC 说话）
//   一次性音效（脚本 play_sound）：不关心位置的 UI 音、爆炸声——不用组件
//
// 【纯数据 + 零依赖】不含 miniaudio 任何类型（char 路径 + POD 字段）：
//   AudioSystem 每帧读它 → 驱动 miniaudio 后端。组件本身可被编辑器
//   反射/序列化（char 数组走手工序列化，同 ScriptComponent 模式）。
//
// 【运行时句柄】soundHandle 是 AudioSystem 分配的内部 ID（0=未创建），
//   不序列化——加载场景后 AudioSystem 惰性创建（同 Mesh/Sprite 句柄）。
// ============================================================
struct AudioComponent
{
    char file[256] = {};  // 音频文件绝对路径（wav/mp3/flac/ogg）
    float volume = 1.0f;  // 音量 0~1
    bool loop = false;    // 循环播放（背景音乐常开）
    bool autoplay = true; // 进入运行模式自动播放（对标 Godot autoplay）
    bool spatial = false; // 3D 空间音频（true=随实体位置衰减，false=2D 全局）
    bool playing = false; // 运行时状态：是否正在播放

    unsigned int soundHandle = 0; // AudioSystem 内部句柄（0=未创建，不序列化）
};

#endif
