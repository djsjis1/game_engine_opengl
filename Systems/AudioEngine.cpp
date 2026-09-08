// ============================================================
// AudioEngine.cpp — AudioSystem 实现（唯一 include miniaudio.h 的地方）
//
// 隔离哲学：miniaudio 的所有类型（ma_engine/ma_sound）只在本文件出现，
// 头文件 AudioSystem.h 零 miniaudio 依赖（pImpl）——与 PhysicsEngine.cpp
// 隔离 Jolt 完全同构。换音频后端只改本文件。
//
// miniaudio 是单头库：必须在【一个】.cpp 里 define 实现宏后 include。
// ★ NOMINMAX：miniaudio 会拉进 Windows.h，它定义 min/max 宏会污染
//   后续 flecs.h 的 std::numeric_limits<U>::min()（Windows 头经典坑）。
// ★ WIN32_LEAN_AND_MEAN：少拉些 Windows 头，减少宏污染面。
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// flecs 相关头在 miniaudio 之后包含也安全了（min/max 宏已被 NOMINMAX 挡掉）
#include "Systems/AudioSystem.h"
#include "Components/AudioComponent.h"
#include "Components/TransformComponent.h"

#include <map>
#include <memory>
#include <vector>
#include <cstring>

// 加载失败哨兵：标记该 AudioComponent 的文件加载失败，别每帧重试（P14a）
static constexpr unsigned int kSoundLoadFailed = 0xFFFFFFFF;

// ============================================================
// Impl：miniaudio 引擎 + 声音句柄表
// ============================================================
struct AudioSystem::Impl
{
    ma_engine engine{};
    bool ready = false;

    // 音频总线（对标 Godot AudioBus）：music/sfx 两组独立音量，
    // 主音量走 engine 自身。ma_sound_group 就是 miniaudio 的总线概念。
    ma_sound_group musicBus{};
    ma_sound_group sfxBus{};
    bool busReady = false;

    // 实体音源：句柄 → (ma_sound + 创建时的文件路径)。
    // 存路径是为了检测 AudioComponent.file 变更后重建（P10）。
    struct EntitySound
    {
        std::unique_ptr<ma_sound> snd;
        std::string file; // 创建时的文件，用于换文件检测
    };
    std::map<unsigned int, EntitySound> sounds;
    unsigned int nextHandle = 1; // 0 保留给"未创建"

    // 一次性音效（play_sound）：fire-and-forget，播完由 update 扫描回收（P4）。
    // 与实体音源分开管理——它们没有宿主实体，生命周期是"播完即弃"。
    std::vector<std::unique_ptr<ma_sound>> oneShots;

    // 背景音乐（单例，切歌自动换）
    std::unique_ptr<ma_sound> music;
};

// 全局单例指针（全局音频接口用，同 g_physicsImpl 模式）
static AudioSystem::Impl *g_audioImpl = nullptr;

// ============================================================
// 生命周期
// ============================================================
void AudioSystem::init()
{
    m_impl = new Impl;
    g_audioImpl = m_impl;
    // ma_engine：miniaudio 高层 API，自动管理设备/混音/解码
    if (ma_engine_init(nullptr, &m_impl->engine) == MA_SUCCESS)
    {
        m_impl->ready = true;
        // 两条总线：music（背景音乐）/ sfx（音效+实体音源）
        m_impl->busReady =
            ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->musicBus) == MA_SUCCESS &&
            ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->sfxBus) == MA_SUCCESS;
    }
}

void AudioSystem::shutdown()
{
    if (!m_impl)
        return;
    stopAll();
    // 先对每个 ma_sound uninit（释放 miniaudio 内部资源），再清容器
    for (auto &kv : m_impl->sounds)
        ma_sound_uninit(kv.second.snd.get());
    m_impl->sounds.clear();
    for (auto &s : m_impl->oneShots)
        ma_sound_uninit(s.get());
    m_impl->oneShots.clear();
    if (m_impl->music)
        ma_sound_uninit(m_impl->music.get());
    if (m_impl->busReady)
    {
        // 总线必须在 sound 全部 uninit 后、engine uninit 前销毁
        ma_sound_group_uninit(&m_impl->musicBus);
        ma_sound_group_uninit(&m_impl->sfxBus);
    }
    if (m_impl->ready)
        ma_engine_uninit(&m_impl->engine);
    g_audioImpl = nullptr;
    delete m_impl;
    m_impl = nullptr;
}

void AudioSystem::stopAll()
{
    if (!m_impl)
        return;
    for (auto &kv : m_impl->sounds)
        ma_sound_stop(kv.second.snd.get());
    for (auto &s : m_impl->oneShots)
        ma_sound_stop(s.get());
    if (m_impl->music)
        ma_sound_stop(m_impl->music.get());
}

// 供 AudioComponent OnRemove observer 调用：宿主实体销毁时停并销毁其音源（P4）
void audioReleaseHandle(unsigned int handle)
{
    if (!g_audioImpl || handle == 0 || handle == kSoundLoadFailed)
        return;
    auto it = g_audioImpl->sounds.find(handle);
    if (it != g_audioImpl->sounds.end())
    {
        ma_sound_uninit(it->second.snd.get());
        g_audioImpl->sounds.erase(it);
    }
}

// ============================================================
// 每帧：驱动实体音源（AudioComponent）
//   惰性创建 → autoplay → 3D 位置同步 → 音量/循环
// ============================================================
void AudioSystem::update(flecs::world &world, float /*dt*/)
{
    if (!isEnabled() || !m_impl || !m_impl->ready)
        return;

    world.each([&](flecs::entity e, AudioComponent &ac, TransformComponent &tf)
               {
        // ① 换文件检测（P10）：已创建但 file 字段和创建时不同 → 销毁重建
        if (ac.soundHandle != 0 && ac.soundHandle != kSoundLoadFailed)
        {
            auto it = m_impl->sounds.find(ac.soundHandle);
            if (it != m_impl->sounds.end() && it->second.file != ac.file)
            {
                ma_sound_uninit(it->second.snd.get());
                m_impl->sounds.erase(it);
                ac.soundHandle = 0; // 归零 → 下面重新惰性创建
            }
        }

        // ② 惰性创建 ma_sound（首次遇到 或 换文件后归零）
        if (ac.soundHandle == 0 && ac.file[0])
        {
            auto snd = std::make_unique<ma_sound>();
            // spatial 音源用 3D 标志，2D 全局音关掉空间化省算力
            ma_uint32 flags = ac.spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
            // 实体音源挂 sfx 总线（set_bus_volume("sfx") 统一控制）
            ma_sound_group *bus = m_impl->busReady ? &m_impl->sfxBus : nullptr;
            if (ma_sound_init_from_file(&m_impl->engine, ac.file, flags,
                                        bus, nullptr, snd.get()) == MA_SUCCESS)
            {
                ma_sound_set_volume(snd.get(), ac.volume);
                ma_sound_set_looping(snd.get(), ac.loop ? MA_TRUE : MA_FALSE);
                unsigned int h = m_impl->nextHandle++;
                ma_sound *raw = snd.get();
                // EntitySound 含 unique_ptr（不可拷贝）→ emplace 直接原地构造，
                // 不能用 operator[]+聚合赋值（会要求拷贝）
                Impl::EntitySound es;
                es.snd = std::move(snd);
                es.file = ac.file; // 记录文件用于换文件检测
                m_impl->sounds.emplace(h, std::move(es));
                ac.soundHandle = h;
                if (ac.autoplay)
                {
                    ma_sound_start(raw);
                    ac.playing = true;
                }
            }
            else
            {
                ac.soundHandle = kSoundLoadFailed; // 标记加载失败，别每帧重试
            }
        }

        // ③ 已创建：同步 3D 位置 + 音量 + 播放状态回读
        auto it = m_impl->sounds.find(ac.soundHandle);
        if (it != m_impl->sounds.end())
        {
            ma_sound *s = it->second.snd.get();
            if (ac.spatial)
                ma_sound_set_position(s, tf.position.x, tf.position.y, tf.position.z);
            ma_sound_set_volume(s, ac.volume);
            // 非循环音播完 → 回读 playing（脚本可查询）
            if (!ac.loop && ma_sound_at_end(s))
                ac.playing = false;
        } });

    // ④ 回收播完的一次性音效（P4）：play_sound 的 fire-and-forget 音效，
    //   播完就 uninit + 移除，否则每放一次泄漏一个 ma_sound（含解码缓冲）
    auto &os = m_impl->oneShots;
    for (auto it = os.begin(); it != os.end();)
    {
        if (ma_sound_at_end(it->get()))
        {
            ma_sound_uninit(it->get());
            it = os.erase(it);
        }
        else
            ++it;
    }
}

// ============================================================
// 全局音频接口（供 Lua API 调用）
// ============================================================

bool audioPlaySound(const std::string &absPath, float volume)
{
    if (!g_audioImpl || !g_audioImpl->ready || absPath.empty())
        return false;
    // 一次性音效：fire-and-forget。放进 oneShots 列表，update 每帧扫描
    // 播完的 → uninit + 移除（P4：之前塞进 sounds map 且从不回收 = 泄漏）
    auto snd = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(&g_audioImpl->engine, absPath.c_str(),
                                MA_SOUND_FLAG_NO_SPATIALIZATION,
                                g_audioImpl->busReady ? &g_audioImpl->sfxBus : nullptr,
                                nullptr, snd.get()) != MA_SUCCESS)
        return false;
    ma_sound_set_volume(snd.get(), volume);
    ma_sound_start(snd.get());
    g_audioImpl->oneShots.push_back(std::move(snd));
    return true;
}

bool audioPlayMusic(const std::string &absPath, float volume)
{
    if (!g_audioImpl || !g_audioImpl->ready || absPath.empty())
        return false;
    // 单例：切歌先停旧的
    if (g_audioImpl->music)
    {
        ma_sound_uninit(g_audioImpl->music.get());
        g_audioImpl->music.reset();
    }
    auto m = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(&g_audioImpl->engine, absPath.c_str(),
                                MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_STREAM,
                                g_audioImpl->busReady ? &g_audioImpl->musicBus : nullptr,
                                nullptr, m.get()) != MA_SUCCESS)
        return false;
    ma_sound_set_volume(m.get(), volume);
    ma_sound_set_looping(m.get(), MA_TRUE); // 背景音乐默认循环
    ma_sound_start(m.get());
    g_audioImpl->music = std::move(m);
    return true;
}

void audioStopMusic()
{
    if (g_audioImpl && g_audioImpl->music)
        ma_sound_stop(g_audioImpl->music.get());
}

void audioSetMasterVolume(float volume)
{
    if (g_audioImpl && g_audioImpl->ready)
        ma_engine_set_volume(&g_audioImpl->engine, volume);
}

// 总线音量（对标 Godot AudioBus）："music"/"sfx"/"master"
void audioSetBusVolume(const std::string &bus, float volume)
{
    if (!g_audioImpl || !g_audioImpl->ready)
        return;
    if (bus == "master")
        ma_engine_set_volume(&g_audioImpl->engine, volume);
    else if (bus == "music" && g_audioImpl->busReady)
        ma_sound_group_set_volume(&g_audioImpl->musicBus, volume);
    else if (bus == "sfx" && g_audioImpl->busReady)
        ma_sound_group_set_volume(&g_audioImpl->sfxBus, volume);
}
