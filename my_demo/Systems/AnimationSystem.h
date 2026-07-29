#ifndef ANIMATIONSYSTEM_H
#define ANIMATIONSYSTEM_H

#include "Core/System.h"
#include "Components/AnimationComponent.h"
#include "Render/AnimationLibrary.h"
#include "Scripting/LuaApi.h" // 事件轨道：触发时推 g_signalQueue（Lua connect 接收）

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

// ============================================================
// AnimationSystem — 属性动画播放（对标 Godot AnimationPlayer 的播放逻辑）
//
// 动画数据在 AnimationLibrary（共享模块）：
//   本系统只读采样；编辑器通过同一个库打关键帧/保存文件
//
// 【Property Track】轨道是反射路径（组件/字段），不限于 Transform：
//   applyClip 通过 flecs 反射写入任意 REFLECT 字段（颜色/血量/fov...）
//
// 【每帧流程】
//   遍历 AnimationComponent：
//     ① time += dt * speed（loop 取模回绕 / 非 loop 到尾停止）
//     ② applyClip(world, entity, clip, time) —— 各轨道反射写入
//
// 【phase=110】脚本(100)之后、Transform传播(150)之前
// 【activeIn=kAllStates】编辑模式勾选 playing 即预览（Godot 编辑器同款）
// ============================================================
class AnimationSystem : public System
{
public:
    int phase() const override { return 110; }
    int activeIn() const override { return kAllStates; }

    void update(flecs::world &world, float dt) override
    {
        world.each([&](flecs::entity e, AnimationComponent &ac)
                   {
            if (!ac.playing || !ac.animFile[0] || !ac.current[0])
                return;

            AnimLib *lib = AnimationLibraryManager::instance().getOrCreate(ac.animFile);
            if (!lib) return;
            auto it = lib->clips.find(ac.current);
            if (it == lib->clips.end())
            {
                warnOnce(std::string("[动画] 找不到动画 '") + ac.current +
                         "'（文件 " + ac.animFile +
                         " 里没有它——检查路径是否正确/文件是否存在/动画名拼写）");
                return;
            }
            AnimClip &clip = it->second;
            if (clip.duration <= 0.0f) return;

            // 推进播放头（记住 prev，事件轨道需要跨越检测）
            float prevTime = ac.time;
            bool wrapped = false;
            ac.time += dt * ac.speed;
            if (ac.loop)
            {
                // 取模回绕（fmod 支持倒放的负数时间）
                float before = ac.time;
                ac.time = std::fmod(ac.time, clip.duration);
                if (ac.time < 0)
                    ac.time += clip.duration;
                wrapped = (before >= clip.duration); // 正放越过尾部回绕
            }
            else if (ac.time >= clip.duration)
            {
                ac.time = clip.duration; // 停在最后一帧
                ac.playing = false;
            }
            else if (ac.time < 0)
            {
                ac.time = 0; // 倒放到头
                ac.playing = false;
            }

            // ★ 事件轨道（Method Track）：跨越检测 → 发信号
            // 走 g_signalQueue 总线（与碰撞信号同一通道）：
            //   AnimationSystem 不认识 Lua —— 只报告"什么时候"，逻辑在脚本
            if (!clip.events.empty() && ac.time != prevTime)
            {
                std::vector<const AnimClip::AnimEvent *> fired;
                clip.collectEvents(prevTime, ac.time, wrapped, fired);
                if (!fired.empty())
                {
                    std::lock_guard<std::mutex> lock(g_signalMutex);
                    for (auto *ev : fired)
                        g_signalQueue.push({e.raw_id(), ev->signal, {}});
                }
            }

            // ★ 反射写入：任意组件的 REFLECT 字段（不限于 Transform）
            applyClip(world, e.raw_id(), clip, ac.time); });
    }

private:
    /// 同一条警告只打印一次（避免每帧刷屏）；帮用户自诊断配置问题
    void warnOnce(const std::string &msg)
    {
        if (m_warned.insert(msg).second)
            fprintf(stderr, "%s\n", msg.c_str());
    }
    std::set<std::string> m_warned;
};

#endif
