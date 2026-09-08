#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "Core/System.h"
#include <string>

// ============================================================
// AudioSystem — miniaudio 封装（ECS System），对标 Godot 音频服务
//
// 职责：
//   1. init()  — 创建 miniaudio 引擎（ma_engine，high-level API）
//   2. update()— 遍历 AudioComponent：惰性创建声音、autoplay、
//                3D 音源同步实体位置、音量/循环变更
//   3. shutdown() — 销毁引擎与所有声音
//
// pImpl 模式：miniaudio.h 只在 AudioEngine.cpp 中 include，编译隔离
//   （与 PhysicsSystem 隔离 Jolt 完全同构）。
//
// 【零 Qt / 零 UI 依赖】音频是纯后端服务，不碰任何 UI 框架——
//   换 UI 库不影响它。这正是"引擎核心与 UI 解耦"的模范。
// ============================================================
class AudioSystem : public System
{
public:
    void init();
    void shutdown() override;
    void update(flecs::world &world, float dt) override;

    /// 停止所有正在播放的声音（退出 Play 时调，防音效漏到编辑模式）
    void stopAll();

    int phase() const override { return 120; }                       // 脚本(100)之后
    int activeIn() const override { return (int)EngineState::Play; } // 只在运行时发声

    struct Impl; // 公开前置声明（供全局音频接口的实现访问）

private:
    Impl *m_impl = nullptr; // pImpl：隔离 miniaudio 头文件
};

// ============================================================
// 全局音频接口（供 Lua API 调用，实现在 AudioEngine.cpp）
//
// 与 physicsApplyImpulse 等同款：脚本层不认识 AudioSystem 类，
// 只调这些自由函数；内部走 g_audioImpl 单例。
// 路径为绝对路径（由 Lua API 侧用 GamePaths 拼好后传入）。
// ============================================================

/// 播放一次性音效（不跟随实体，用完自动回收）。返回是否成功
bool audioPlaySound(const std::string &absPath, float volume);

/// 播放/切换背景音乐（循环，单例——新音乐自动停掉旧的）
bool audioPlayMusic(const std::string &absPath, float volume);

/// 停止背景音乐
void audioStopMusic();

/// 设置主音量（0~1，影响全部声音）
void audioSetMasterVolume(float volume);

/// 总线音量（对标 Godot AudioBus）："music"=背景音乐组 "sfx"=音效组 "master"=总
void audioSetBusVolume(const std::string &bus, float volume);

/// 释放某实体音源句柄（AudioComponent OnRemove observer 调用，防死亡实体声音永播）
void audioReleaseHandle(unsigned int handle);

#endif
