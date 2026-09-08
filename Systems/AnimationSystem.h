#ifndef ANIMATIONSYSTEM_H
#define ANIMATIONSYSTEM_H

#include "Core/System.h"                   // System 基类（phase/activeIn/update 接口）
#include "Core/SignalQueue.h"              // 事件轨道：触发时推信号队列（Core 层，不依赖脚本，ADR-002）
#include "Components/AnimationComponent.h" // "播放器"组件（time/playing/loop/speed）
#include "Render/AnimationLibrary.h"       // "音乐库"（AnimClip 数据 + applyClip 反射写入）

#include <cmath>  // std::fmod（循环回绕用）
#include <cstdio> // fprintf（诊断警告用）
#include <set>    // 警告去重集合
#include <string>

// ============================================================
// AnimationSystem — 属性动画播放（对标 Godot AnimationPlayer 的播放逻辑）
//
// 【一句话职责】每帧对每个"开着的播放器"做三件事：
//   ① 走针（time 前进）
//   ② 针出界处理（循环回绕 / 停在末帧）
//   ③ 按针的位置取值写进组件字段（applyClip 反射写入）
//
// 【数据在哪】动画数据（歌）在 AnimationLibrary 共享库里，本系统只读；
//   播放状态（进度条）在每个实体自己的 AnimationComponent 里。
//   → 100 个实体播同一动画：数据 1 份，进度条 100 个，互不干扰。
//
// 【phase=110 为什么】执行顺序 = Physics(90) → Script(100) → 本系统(110)
//   → Transform传播(150) → 渲染。脚本先跑：脚本里 anim_play() 切的动画
//   本帧就能生效；Transform 后跑：动画写的 position 本帧就进世界矩阵。
//
// 【activeIn=kAllStates 为什么】编辑模式也要跑——编辑器勾"播放（预览）"
//   就是把 playing 设 true，靠本系统在编辑模式驱动预览（Godot 同款）。
// ============================================================
class AnimationSystem : public System
{
public:
    int phase() const override { return 110; }
    int activeIn() const override { return kAllStates; }

    void update(flecs::world &world, float dt) override
    {
        // 遍历所有带 AnimationComponent 的实体（e=实体，ac=它的"播放器"）
        // 注意 ac 是引用：下面对 ac.time 的修改直接写回组件
        world.each([&](flecs::entity e, AnimationComponent &ac)
                   {
            // ---- 守卫：三种"不用干活"的情况，第一行就出门 ----
            //   !ac.playing      → 播放器没开
            //   !ac.animFile[0]  → 没配动画文件（char数组首字符为0 = 空字符串）
            //   !ac.current[0]   → 没选要播哪个动画
            if (!ac.playing || !ac.animFile[0] || !ac.current[0])
                return; // 注：each 的 lambda 里 return = 跳过本实体，不是退出整个循环

            // ---- 从音乐库拿"歌" ----
            // getOrCreate：已加载直接从缓存拿（不会每帧读磁盘！只有首次才读文件）
            AnimLib *lib = AnimationLibraryManager::instance().getOrCreate(ac.animFile);
            if (!lib) return; // 路径为空串等极端情况
            // 在库里按名字找 clip（map 查找）
            auto it = lib->clips.find(ac.current);
            if (it == lib->clips.end())
            {
                // 找不到 = 配置错误（路径错/名字拼错/文件是旧格式）
                // 打警告帮用户排查，但只打一次（见 warnOnce），然后跳过
                warnOnce(std::string("[动画] 找不到动画 '") + ac.current +
                         "'（文件 " + ac.animFile +
                         " 里没有它——检查路径是否正确/文件是否存在/动画名拼写）");
                return;
            }
            AnimClip &clip = it->second;
            if (clip.duration <= 0.0f) return; // 时长非法：除零/取模保护

            // ================================================
            // ① 走针 + ② 出界处理
            // ================================================

            // 先记住走针前的位置！事件轨道要靠 (prevTime → ac.time) 这段
            // "区间"判断跨过了哪些事件时刻（不能用 == 判断，见下方事件段）
            float prevTime = ac.time;
            bool wrapped = false; // 本帧是否发生了"越过尾部绕回开头"

            // 走针：进度 += 时间 × 倍率
            //   speed=1 正常速 │ speed=2 两倍速 │ speed=-1 倒放（time 变小）
            ac.time += dt * ac.speed;

            if (ac.loop)
            {
                // ---- 循环模式：出界就绕回 ----
                float before = ac.time; // 取模前的值（判断 wrapped 用）

                // fmod = 浮点取余。例：fmod(2.3, 2.0)=0.3 → 越过尾部绕回开头
                ac.time = std::fmod(ac.time, clip.duration);

                // ⚠ fmod 的坑：负数结果保留符号！fmod(-0.3, 2.0) = -0.3（不是1.7）
                // 倒放越过开头时 time 是负的 → 手动补一个周期变成从尾部绕回
                if (ac.time < 0)
                    ac.time += clip.duration; // -0.3 + 2.0 = 1.7 ✓

                // 正放越过尾部才算 wrapped（before 已 >= 时长说明这帧跨过了终点）
                // collectEvents 靠它知道要把检测区间拆成"尾部一段+头部一段"
                wrapped = (before >= clip.duration);
            }
            else if (ac.time >= clip.duration)
            {
                // ---- 单次模式，正放到尾 ----
                // 钉在最后一帧而不是跳回 0：像"开门"动画播完门要保持开着
                ac.time = clip.duration;
                ac.playing = false; // 自动停机（下帧第一行守卫就出门了）
            }
            else if (ac.time < 0)
            {
                // ---- 单次模式，倒放到头 ----
                ac.time = 0;
                ac.playing = false;
            }

            // ================================================
            // ★ 事件轨道（Method Track）：跨越检测 → 发信号
            // ================================================
            // 为什么不能写 if (ac.time == 事件时刻)？
            //   播放头是跳着走的：这帧 0.49 → 下帧 0.51，永远不等于 0.50！
            //   必须问"这一步有没有跨过 0.50"（区间检测 (prev, cur]）
            //
            // 为什么发信号而不是直接调函数？——解耦：
            //   本系统不认识 Lua，只报告"什么时候到了"；
            //   "到了该干什么"由脚本 connect("信号名", 回调) 决定。
            //   信号走 g_signalQueue 总线（和物理碰撞信号同一条队列），
            //   下一帧被 ScriptSystem 消费 → 调 Lua 回调（有一帧延迟，无感知）
            if (!clip.events.empty() && ac.time != prevTime)
            {
                std::vector<const AnimClip::AnimEvent *> fired; // 本帧跨过的事件们
                clip.collectEvents(prevTime, ac.time, wrapped, fired);
                for (auto *ev : fired)
                    // {发射者实体, 信号名, 无参数}（内部加锁，线程安全）
                    pushDeferredSignal({e.raw_id(), ev->signal, {}});
            }

            // ================================================
            // ③ 按针取值 → 写进组件字段（本系统的最终输出）
            // ================================================
            // 过渡中混合两个动画（crossfade），否则单动画（零额外开销）
            if (ac.blendDuration > 0.0f && ac.prev[0])
            {
                ac.prevTime += dt * ac.speed; // 旧动画的播放头继续推进
                ac.blendTime += dt;
                float w = ac.blendTime / ac.blendDuration; // 权重 0→旧 1→新
                auto pit = lib->clips.find(ac.prev);
                if (w >= 1.0f || pit == lib->clips.end())
                {
                    // 过渡完成（或旧动画丢失）→ 收收尾，回到单动画
                    ac.blendDuration = 0.0f;
                    ac.prev[0] = '\0';
                    applyClip(world, e.raw_id(), clip, ac.time);
                }
                else
                {
                    // 旧 clip 循环包裹（防 prevTime 越界），两动画加权混合
                    AnimClip &from = pit->second;
                    float ft = from.duration > 0 ? std::fmod(ac.prevTime, from.duration) : 0;
                    if (ft < 0) ft += from.duration;
                    applyClipBlended(world, e.raw_id(), from, ft, clip, ac.time, w);
                }
            }
            else
            {
                // 单动画：任意组件的 REFLECT 字段（不限于 Transform）
                applyClip(world, e.raw_id(), clip, ac.time);
            } });
    }

private:
    /// 同一条警告只打印一次（不去重的话 60帧/秒 × 每帧一条 = 刷屏灾难）
    /// insert().second：set 插入成功（=第一次见）返回 true → 才打印
    void warnOnce(const std::string &msg)
    {
        if (m_warned.insert(msg).second)
            fprintf(stderr, "%s\n", msg.c_str());
    }
    std::set<std::string> m_warned; // 已打印过的警告集合
};

#endif
