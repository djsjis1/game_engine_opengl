#ifndef ANIMATIONLIBRARY_H
#define ANIMATIONLIBRARY_H

#include "Core/Easing.h" // 缓动算法库（12种曲线+自定义贝塞尔，纯数学零依赖）

// flecs：applyClip 反射写入需要查 EcsStruct 元数据
// emit 宏保护：Qt 定义了 emit 宏，会破坏 flecs 源码里的 emit 变量名（MODULES.md 坑1）
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include <nlohmann/json.hpp> // .anim 文件是 JSON 格式
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cmath>

// ============================================================
// AnimationLibrary — 动画数据中心（编辑器与播放系统共享）
//
// 【音乐播放器比喻】（贯穿全文件的心智模型）
//   AnimKeyframe = 音符（什么时候、是多少）
//   AnimTrack    = 音轨（一串音符 + "管谁"的地址标签）
//   AnimClip     = 歌（时长 + 几条音轨 + 事件点）
//   AnimLib      = 专辑（一个 .anim 文件里的所有歌）
//   Manager      = 音乐库管理员（加载/缓存/存盘）
//   ※"播放器"（进度条 time/playing）不在本文件！在每个实体的
//     AnimationComponent 里——歌只存一份，进度条每实体一个。
//
// 【为什么独立成模块】
//   动画数据有两个消费者，职责完全不同：
//     AnimationSystem（运行时）→ 只读采样
//     EditorPanel（编辑器）    → 读写：打关键帧/新建clip/保存文件
//   数据若藏在 System 私有缓存里，编辑器就做不了动画工具 →
//   抽出共享库，编辑/播放彻底解耦（对标 Godot Animation 资源独立于 AnimationPlayer）
//
// 【Property Track（对标 Godot）】
//   轨道目标不是硬编码的 position/rotation/scale，而是反射路径：
//     "TransformComponent" / "position"   → 位移动画
//     "MaterialComponent"  / "diffuse"    → 颜色渐变
//     "HealthComponent"    / "hp"         → 血量倒计时
//   采样时通过 flecs 反射（EcsStruct 元数据）找到字段内存位置直接写入。
//   任何 REFLECT 标记的字段自动可动画 —— 动画×反射两大系统的正交组合，
//   这就是 Godot "任何属性都能动画"的底层原理。
//
// 【零代码工作流】（编辑器面板驱动）
//   拖时间滑块 → 摆姿势 → setKey() 打关键帧 → save() 写 .anim 文件
// ============================================================

// ============================================================
// AnimKeyframe — 音符：动画的最小单位
//
// "动画"不存每一帧的画面，只存几个关键时刻的值，中间靠插值算。
// 两个关键帧（16字节×2）就能描述"2秒从左移到右"——这就是省内存的原理。
// ============================================================
struct AnimKeyframe
{
    float t;     // 什么时候（秒）
    glm::vec3 v; // 是多少。★统一用 vec3 存所有类型：
                 //   vec3 字段（position/颜色）→ 三个分量全用
                 //   float 字段（hp/fov）      → 只用 v.x
                 //   bool 字段（visible）      → v.x > 0.5 算 true
                 //   int 字段（frame 帧号）    → v.x 四舍五入
                 // 统一存储换代码简单：插值/序列化只需写一份 vec3 版

    int easing = 0; // 本帧→下一帧这段路用什么曲线走（EasingType，0=线性）
                    // 挂在"帧"上而非"轨道"上：每段可用不同曲线
                    // （Godot 同款 per-key transition）

    // 自定义贝塞尔控制点（仅 easing==kCustomBezier 时使用）
    // 4 个数字定义一条任意曲线（CSS cubic-bezier 同款机制）
    // 默认值 = CSS 的 "ease" 曲线
    float bz[4] = {0.25f, 0.1f, 0.25f, 1.0f};
};

// ============================================================
// AnimTrack — 音轨：一串音符 + "管谁"的地址标签
//
// 一个动画往往同时控制多个属性（位置在变+颜色在变），
// 每个属性各一条独立音轨，同一时刻各自插值、各写各的字段。
// ============================================================
struct AnimTrack
{
    // ---- 地址标签（Property Track 的反射路径）----
    // ★为什么存字符串而不是指针/类型？——因为要存进 .anim 文件！
    //   指针是内存里的东西，程序一关就失效；字符串能落盘，
    //   重启后由下面的"翻译缓存"机制重新翻译成内存地址。
    std::string comp;  // 管哪个组件，如 "TransformComponent"
    std::string field; // 管它的哪个字段，如 "position"

    std::vector<AnimKeyframe> keys; // 音符们。★始终按时间升序！
                                    // （setKey 负责维护，sample 依赖此前提）

    // ---- 运行时翻译缓存 ----
    // 字符串标签 → 内存地址的"翻译结果"。翻译（查反射表）是慢操作，
    // applyClip 第一次用到本轨道时翻译一次存这里，以后每帧直接用数字。
    // ※不序列化：组件 ID 每次运行都可能不同，每次启动后重新翻译。
    enum FieldKind
    {
        kUnresolved = 0, // 还没翻译过（初始状态）
        kInvalid,        // 翻译失败（组件/字段名不存在）→ 永久跳过不再重试
        kVec3,           // 字段是 glm::vec3（12字节）
        kFloat,          // 字段是 float（4字节，取 v.x）
        kBool,           // 字段是 bool（v.x>0.5）
        kInt32,          // 字段是 int32（v.x 四舍五入——帧动画的阶梯感靠它）
    };
    FieldKind kind = kUnresolved;
    ecs_entity_t compId = 0; // 翻译结果①：组件类型的数字 ID
    int32_t offset = 0;      // 翻译结果②：字段在组件内存里的字节偏移

    /// 编辑器改了 comp/field 后调用：作废翻译缓存，下次使用时重新翻译
    void invalidate() { kind = kUnresolved; }

    /// 打关键帧：同一时刻（±10ms）已有帧则覆盖，否则按时间序插入
    /// bz：自定义贝塞尔 4 参数（仅 easing==kCustomBezier 时有意义，可传 nullptr）
    void setKey(float t, const glm::vec3 &v, int easing = 0, const float *bz = nullptr)
    {
        // 先找"同一时刻"的已有帧
        // ★浮点数不能用 == 比较！滑块拖出来的是 0.30000001 这种脏数字，
        //   fabs(差)<容差 是标准写法。±10ms 内视为同一时刻 →
        //   行为变成"覆盖更新"：编辑器里重打帧 = 修正而不是叠加（Godot 同行为）
        for (auto &k : keys)
        {
            if (std::fabs(k.t - t) < 0.01f)
            {
                k.v = v; // 覆盖
                k.easing = easing;
                if (bz)
                    for (int i = 0; i < 4; ++i)
                        k.bz[i] = bz[i];
                return;
            }
        }
        // 没有同时刻帧 → 新建一个
        AnimKeyframe nk{t, v, easing};
        if (bz)
            for (int i = 0; i < 4; ++i)
                nk.bz[i] = bz[i];
        // 找插入位置：跳过所有时间比 t 早的帧
        // → 保证 keys 永远升序（sample 的二帧查找依赖这个前提）
        auto it = keys.begin();
        while (it != keys.end() && it->t < t)
            ++it;
        keys.insert(it, nk);
    }

    /// 删除指定下标的关键帧（编辑器"删除选中关键帧"按钮用）
    void removeKeyAt(int index)
    {
        if (index >= 0 && index < (int)keys.size())
            keys.erase(keys.begin() + index);
    }

    /// 轨道显示名（编辑器关键帧列表用）："组件名/字段名"
    std::string trackName() const { return comp + "/" + field; }

    // ========================================================
    // sample — 采样：给一个时刻，算出这一刻的值（本文件的数学心脏）
    //
    // 数值例：keys = {0s→0} {2s→360}，问 time=0.506：
    //   找到夹住它的两帧 a(0s→0)、b(2s→360)
    //   span  = 2-0 = 2          这段路多长
    //   alpha = 0.506/2 = 0.253  走了 25.3%（归一化进度）
    //   缓动：线性时 alpha 不变形；弹跳曲线会把 0.253 扭成别的数
    //        （★扭的是"进度"不是"值"——所以缓动和 lerp 互不认识，解耦）
    //   结果  = 0 + (360-0)×0.253 = 91.08   ← lerp：起点+路程×进度
    // ========================================================
    glm::vec3 sample(float time) const
    {
        if (keys.empty())
            return glm::vec3(0); // 没音符：返回零值（防崩底线）

        // ---- 边界钳制（Godot 同行为）----
        // 播放头不保证落在首末帧之间：首帧打在 0.3s 而播放头在 0.1s？
        // → 保持首帧值。不钳制的话下面循环会算出 alpha>1，值飞出去。
        if (time <= keys.front().t)
            return keys.front().v; // 早于首帧 → 钉在首帧值
        if (time >= keys.back().t)
            return keys.back().v; // 晚于末帧 → 钉在末帧值

        // ---- 找到夹住 time 的两帧 a、b ----
        // keys 升序，从前往后找第一个"比 time 晚"的帧 = b，前一个 = a
        for (size_t i = 1; i < keys.size(); ++i)
        {
            if (time < keys[i].t)
            {
                const AnimKeyframe &a = keys[i - 1];
                const AnimKeyframe &b = keys[i];
                float span = b.t - a.t;
                // span>0 保护：两帧时间相同时防除零
                float alpha = span > 0 ? (time - a.t) / span : 0;

                // ★ 缓动：线性进度 → 曲线进度（算法由起始帧 a 指定，策略模式）
                // 自定义贝塞尔 = 用户用 4 个参数定义的专属曲线（数据驱动）
                if (a.easing == (int)EasingType::kCustomBezier)
                    alpha = easeBezier(a.bz[0], a.bz[1], a.bz[2], a.bz[3], alpha);
                else
                    alpha = ease(a.easing, alpha);

                return a.v + (b.v - a.v) * alpha; // lerp：起点 + 路程 × 进度
            }
        }
        return keys.back().v; // 理论到不了（上面钳制拦截了），保险
    }
};

// ============================================================
// AnimClip — 歌：时长 + 几条属性音轨 + 事件点
// ============================================================
struct AnimClip
{
    float duration = 1.0f;         // 总时长（秒）。播放头出界处理靠它
    std::vector<AnimTrack> tracks; // 属性音轨们（各管一个字段，互不干扰）

    // ---- 事件轨道（Method Track，对标 Godot）----
    // "播到 0.5 秒时触发 attack_hit" —— 播放头跨越事件时刻 → 发射信号
    // （走 Core 层信号总线，Lua connect("attack_hit",cb) 接收）
    // ★动画不直接调函数 —— 解耦：动画只负责"什么时候"，"干什么"在脚本
    struct AnimEvent
    {
        float t;            // 触发时刻（秒）
        std::string signal; // 信号名（如 "attack_hit"、"play_sound"）
    };
    std::vector<AnimEvent> events; // 按 t 升序（addEvent 维护）

    /// 添加事件帧（同时刻同名视为重复不再加，按时间序插入）
    void addEvent(float t, const std::string &signal)
    {
        for (auto &ev : events)
            if (std::fabs(ev.t - t) < 0.01f && ev.signal == signal)
                return; // 已存在（同样的浮点容差技巧）
        auto it = events.begin();
        while (it != events.end() && it->t < t)
            ++it;
        events.insert(it, {t, signal});
    }

    /// 删除指定下标的事件（编辑器"删除选中事件"按钮用）
    void removeEventAt(int index)
    {
        if (index >= 0 && index < (int)events.size())
            events.erase(events.begin() + index);
    }

    // ========================================================
    // collectEvents — ★跨越检测：收集播放头这一步跨过了哪些事件
    //
    // 为什么不能写 if (time == 事件时刻)？
    //   播放头是跳着走的：这帧 0.49 → 下帧 0.51，永远不等于 0.50！
    //   必须问"prev→cur 这一步有没有跨过 0.50"（区间检测）。
    //
    // 三种情况（配数值例，duration=2.0）：
    //   正放：prev=0.49 cur=0.51 → 事件在半开区间 (0.49, 0.51] 内算跨过
    //        （prev 端不含：上一帧已经含过了，防止重复触发两次）
    //   回绕：prev=1.98 cur=0.01（loop 取模绕回）→ 拆两段检测：
    //        (1.98, 2.0] 尾巴段 + [0, 0.01] 开头段
    //        怎么知道回绕了？靠调用方（AnimationSystem）传的 wrapped 标志
    //   倒放：prev=0.51 cur=0.49（speed<0）→ 区间反向 [0.49, 0.51)
    // ========================================================
    void collectEvents(float prev, float cur, bool wrapped,
                       std::vector<const AnimEvent *> &out) const
    {
        // 半开区间判断 (lo, hi]
        auto inRange = [](float t, float lo, float hi)
        { return t > lo && t <= hi; };
        for (const auto &ev : events)
        {
            bool hit = false;
            if (wrapped)
                // 回绕：尾巴段 或 开头段，命中任一即触发
                hit = inRange(ev.t, prev, duration + 0.001f) || (ev.t >= 0 && ev.t <= cur);
            else if (cur >= prev)
                // 正放：普通区间
                hit = inRange(ev.t, prev, cur);
            else
                // 倒放：区间反向
                hit = ev.t >= cur && ev.t < prev;
            if (hit)
                out.push_back(&ev);
        }
    }

    /// 找到（或创建）指定反射路径的轨道 —— get-or-create 惯用法
    /// 调用方一行搞定：clip->track("TransformComponent","position").setKey(...)
    /// 不用先判断轨道存不存在（把 if 分支埋进 API，调用方永远简洁）
    AnimTrack &track(const std::string &comp, const std::string &field)
    {
        for (auto &t : tracks)
            if (t.comp == comp && t.field == field)
                return t; // 找到 → 直接给
        AnimTrack t;
        t.comp = comp;
        t.field = field;
        tracks.push_back(std::move(t));
        return tracks.back(); // 没有 → 现场建一条
    }
};

// ============================================================
// applyClip — ⭐反射写入：把字符串标签变成真实的内存写入
//
// 对 clip 里每条轨道做三段事：
//   ① 惰性翻译：字符串标签 → (组件ID, 字节偏移, 类型)，只翻一次就缓存
//   ② 安全门：翻译失败/实体没挂该组件 → 跳过这条轨道（不崩、不越权）
//   ③ 采样+指针算术：sample(time) 算出值 → data+offset → 按类型写入
//
// 运行时播放（AnimationSystem）和编辑器 scrub 预览共用本函数。
//
// 内存图（以 "MaterialComponent/diffuse"、offset=12 为例）：
//   MaterialComponent: [ambient 12字节][diffuse 12字节][specular...]
//                       0              12              24
//   data ──────────────▶│
//            + offset=12└─▶ fieldPtr，往这里写 12 个字节 → 材质变色
// ============================================================
inline void applyClip(flecs::world &w, ecs_entity_t entity, AnimClip &clip, float time)
{
    for (auto &tr : clip.tracks)
    {
        // ============================================
        // ① 惰性翻译（只有第一次进来才真正执行）
        // ============================================
        if (tr.kind == AnimTrack::kUnresolved)
        {
            // ★先假定失败：若中途任何一步失败（组件名/字段名拼错），
            //   kind 停留在 kInvalid → 以后每帧直接跳过，不会反复重试
            //   翻译字符串（失败也要缓存，这是性能细节）
            tr.kind = AnimTrack::kInvalid;

            // "MaterialComponent" → 数字 ID
            // （组件类型在 flecs 里也是实体，按名字查得到——"用 ECS 描述 ECS"）
            tr.compId = ecs_lookup(w.c_ptr(), tr.comp.c_str());
            if (tr.compId)
            {
                // 取该组件的"字段清单"（EcsStruct）
                // ——就是 REFLECT 宏 → reflect_gen → member() 注册进去的那张表
                const EcsStruct *st = (const EcsStruct *)ecs_get_id(
                    w.c_ptr(), tr.compId, ecs_id(EcsStruct));
                if (st)
                {
                    // 遍历字段清单，按名字找 "diffuse"
                    auto *members = (const ecs_member_t *)ecs_vec_first(&st->members);
                    int32_t count = ecs_vec_count(&st->members);
                    for (int32_t i = 0; i < count; ++i)
                    {
                        if (tr.field != members[i].name)
                            continue;
                        // 找到了！记住两个翻译结果：
                        tr.offset = members[i].offset; // 字节偏移
                        ecs_entity_t ft = members[i].type;
                        // 按字段类型定 kind（决定后面怎么写内存）
                        if (ft == ecs_id(ecs_f32_t))
                            tr.kind = AnimTrack::kFloat;
                        else if (ft == ecs_id(ecs_bool_t))
                            tr.kind = AnimTrack::kBool;
                        else if (ft == ecs_id(ecs_i32_t))
                            tr.kind = AnimTrack::kInt32;
                        else if (ecs_get_id(w.c_ptr(), ft, ecs_id(EcsStruct)))
                            tr.kind = AnimTrack::kVec3; // 结构体字段：目前即 glm::vec3
                        break;
                    }
                }
            }
        }

        // ============================================
        // ② 两道安全门
        // ============================================
        // 门1：翻译失败的轨道 / 没音符的轨道 → 跳过
        if (tr.kind == AnimTrack::kInvalid || tr.keys.empty())
            continue;

        // 门2：这个实体压根没挂该组件 → 跳过
        // ★必须检查！不查的话下面 ecs_get_mut_id 会偷偷给实体
        //   自动添加组件（get_mut 的语义）——动画不该改变实体结构。
        //   同一动画给不同实体用：有 HealthComponent 的血条轨生效，
        //   没有的只是这条轨不生效，其他轨照常（continue 只跳过一条
        //   轨道不放弃整个动画——故障隔离）
        if (!ecs_has_id(w.c_ptr(), entity, tr.compId))
            continue;
        void *data = ecs_get_mut_id(w.c_ptr(), entity, tr.compId); // 组件数据起始地址（可写）
        if (!data)
            continue;

        // ============================================
        // ③ 采样 + 指针算术写入
        // ============================================
        glm::vec3 v = tr.sample(time); // 插值算出这一刻的值

        // ★指针算术必转 char*：char 是 1 字节，data+offset 才是"前进 offset 字节"
        //  （若用 vec3* 加法会前进 offset×12 字节——灾难）
        char *fieldPtr = (char *)data + tr.offset;

        // 按字段类型分发写入（v 统一是 vec3，见 AnimKeyframe.v 的注释）
        switch (tr.kind)
        {
        case AnimTrack::kVec3:
            *(glm::vec3 *)fieldPtr = v; // 写 12 字节（position/颜色/缩放...）
            break;
        case AnimTrack::kFloat:
            *(float *)fieldPtr = v.x; // 只取 x 写 4 字节（hp/fov...）
            break;
        case AnimTrack::kBool:
            *(bool *)fieldPtr = v.x > 0.5f; // 阈值转布尔（visible...）
            break;
        case AnimTrack::kInt32:
            // ★lround 四舍五入而不是 C 截断：帧号轨道 0→4 连续插值出 1.37、
            //   2.99...，取整后变成阶梯 0,0,1,1,2,2 —— 帧动画的逐格翻页感！
            //   若用截断 (int)2.99=2，切换时刻会滞后且受浮点误差影响
            *(int32_t *)fieldPtr = (int32_t)std::lround(v.x);
            break;
        default:
            break;
        }
    }
}

// ============================================================
// resolveTrack — 惰性解析反射路径（applyClip 与混合版共用）
// 将轨道的 comp/field 字符串 → compId + offset + kind（首次解析后缓存）
// ============================================================
inline void resolveTrack(flecs::world &w, AnimTrack &tr)
{
    if (tr.kind != AnimTrack::kUnresolved)
        return;
    tr.kind = AnimTrack::kInvalid;
    tr.compId = ecs_lookup(w.c_ptr(), tr.comp.c_str());
    if (!tr.compId)
        return;
    const EcsStruct *st = (const EcsStruct *)ecs_get_id(
        w.c_ptr(), tr.compId, ecs_id(EcsStruct));
    if (!st)
        return;
    auto *members = (const ecs_member_t *)ecs_vec_first(&st->members);
    int32_t count = ecs_vec_count(&st->members);
    for (int32_t i = 0; i < count; ++i)
    {
        if (tr.field != members[i].name)
            continue;
        tr.offset = members[i].offset;
        ecs_entity_t ft = members[i].type;
        if (ft == ecs_id(ecs_f32_t))
            tr.kind = AnimTrack::kFloat;
        else if (ft == ecs_id(ecs_bool_t))
            tr.kind = AnimTrack::kBool;
        else if (ft == ecs_id(ecs_i32_t))
            tr.kind = AnimTrack::kInt32;
        else if (ecs_get_id(w.c_ptr(), ft, ecs_id(EcsStruct)))
            tr.kind = AnimTrack::kVec3;
        break;
    }
}

// 按 kind 把 vec3 值写入字段地址（提取自 applyClip 的 switch）
inline void writeField(AnimTrack::FieldKind kind, char *fieldPtr, const glm::vec3 &v)
{
    switch (kind)
    {
    case AnimTrack::kVec3:
        *(glm::vec3 *)fieldPtr = v;
        break;
    case AnimTrack::kFloat:
        *(float *)fieldPtr = v.x;
        break;
    case AnimTrack::kBool:
        *(bool *)fieldPtr = v.x > 0.5f;
        break;
    case AnimTrack::kInt32:
        *(int32_t *)fieldPtr = (int32_t)std::lround(v.x);
        break;
    default:
        break;
    }
}

// ============================================================
// applyClipBlended — 两个动画加权混合写入（对标 Godot crossfade）
//
// 以 to 为主遍历其轨道：
//   对每条轨道，采样 to 的值 vTo；在 from 里找同路径轨道采样 vFrom；
//   混合值 = vFrom*(1-w) + vTo*w  → 写字段
//   from 里没有的轨道：直接用 vTo（从组件当前值渐变到 vTo 由权重控）
// weight：0=完全 from，1=完全 to（过渡进度）
// ============================================================
inline void applyClipBlended(flecs::world &w, ecs_entity_t entity,
                             AnimClip &from, float fromTime,
                             AnimClip &to, float toTime, float weight)
{
    for (auto &tr : to.tracks)
    {
        resolveTrack(w, tr);
        if (tr.kind == AnimTrack::kInvalid || tr.keys.empty())
            continue;
        if (!ecs_has_id(w.c_ptr(), entity, tr.compId))
            continue;
        void *data = ecs_get_mut_id(w.c_ptr(), entity, tr.compId);
        if (!data)
            continue;

        glm::vec3 vTo = tr.sample(toTime);

        // 在 from 里找同路径轨道（同一属性才能插值，position 与 position 混）
        glm::vec3 vFrom = vTo; // 默认：from 无此轨道则不混（结果=vTo）
        for (auto &ft : from.tracks)
            if (ft.comp == tr.comp && ft.field == tr.field)
            {
                vFrom = ft.sample(fromTime);
                break;
            }

        glm::vec3 mixed = vFrom * (1.0f - weight) + vTo * weight; // lerp
        writeField(tr.kind, (char *)data + tr.offset, mixed);
    }
}

// ============================================================
// AnimLib — 专辑：一个 .anim 文件的全部内容（名字 → 歌）
// ============================================================
struct AnimLib
{
    std::map<std::string, AnimClip> clips; // "spin"→clip、"bounce"→clip...
};

// ============================================================
// AnimationLibraryManager — 音乐库管理员（单例）
//
// 核心是一个缓存：map<文件路径, AnimLib>
//   加载：getOrCreate（首次读文件解析，之后纯内存查 map——
//         所以 AnimationSystem 每帧调用也不亏，不会每帧读磁盘）
//   编辑：编辑器/Lua 直接改缓存里的库（内存里改，立即可播）
//   存盘：save 把内存库写回 .anim 文件（点"保存"按钮才落盘）
// ============================================================
class AnimationLibraryManager
{
public:
    /// Meyers 单例：全局唯一实例（首次调用时构造，线程安全）
    static AnimationLibraryManager &instance()
    {
        static AnimationLibraryManager m;
        return m;
    }

    // ========================================================
    // getOrCreate — 取动画库（缓存加载）
    //
    // ★"文件不存在也返回空库"是故意的（Null Object 模式）：
    //   编辑器里填一个还不存在的文件名 → 拿到空库直接开始新建动画
    //   打帧 → 点保存时文件才诞生。空库对所有代码都是合法对象
    //   （clips 为空的 map，遍历零次而已），没人需要判空特殊处理。
    // ========================================================
    AnimLib *getOrCreate(const std::string &path)
    {
        if (path.empty())
            return nullptr; // 路径空串是唯一返回 null 的情况

        // 缓存命中 → 直接给（每帧调用走的都是这条快路径）
        auto it = m_cache.find(path);
        if (it != m_cache.end())
            return &it->second;

        // 未命中 → 先在缓存里占位一个空库（文件无效时它就是结果）
        AnimLib &lib = m_cache[path];
        std::ifstream in(path);
        if (in)
        {
            // parse 第三个参数 false = 解析失败不抛异常，返回 discarded
            nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
            if (!doc.is_discarded() && doc.contains("clips"))
                parseInto(lib, doc); // 文件合法才解析进库
        }
        return &lib;
    }

    // ========================================================
    // save — 内存库写回 .anim 文件（编辑器「保存动画」按钮）
    //
    // 与 parseInto 互为镜像：内存结构 ⇄ JSON 字段一一对应。
    // 小设计：默认值不写文件（easing=0 线性就不写 "e" 字段）
    //   → 文件更小更干净；读取端 value("e",0) 给同样的默认值。
    // ========================================================
    bool save(const std::string &path)
    {
        auto it = m_cache.find(path);
        if (it == m_cache.end())
            return false; // 从没加载/创建过这个库，没东西可存
        nlohmann::json doc;
        for (auto &[name, clip] : it->second.clips) // 每首歌
        {
            nlohmann::json cj;
            cj["duration"] = clip.duration;
            cj["tracks"] = nlohmann::json::array();
            for (auto &tr : clip.tracks) // 每条音轨
            {
                nlohmann::json tj;
                tj["comp"] = tr.comp;   // 反射路径：组件名
                tj["field"] = tr.field; // 反射路径：字段名
                // ※注意 kind/compId/offset 翻译缓存不写文件（运行时重建）
                tj["keys"] = nlohmann::json::array();
                for (auto &k : tr.keys) // 每个音符
                {
                    nlohmann::json kj = {{"t", k.t}, {"v", {k.v.x, k.v.y, k.v.z}}};
                    if (k.easing != 0)
                        kj["e"] = k.easing; // 缓动类型（缺省线性不写，文件更紧凑）
                    if (k.easing == (int)EasingType::kCustomBezier)
                        kj["bz"] = {k.bz[0], k.bz[1], k.bz[2], k.bz[3]}; // 自定义曲线参数
                    tj["keys"].push_back(kj);
                }
                cj["tracks"].push_back(tj);
            }
            // 事件轨道（Method Track）
            if (!clip.events.empty())
            {
                cj["events"] = nlohmann::json::array();
                for (auto &ev : clip.events)
                    cj["events"].push_back({{"t", ev.t}, {"s", ev.signal}});
            }
            doc["clips"][name] = cj;
        }
        std::ofstream out(path);
        if (!out)
            return false;   // 目录不存在/无写权限
        out << doc.dump(2); // 缩进 2 格的漂亮格式（方便人看）
        return true;
    }

private:
    AnimationLibraryManager() = default; // 单例：外界不能 new

    // ========================================================
    // parseInto — JSON → 内存（save 的镜像）
    //
    // 全程用 value(key, 默认值) 而不是 j[key]：
    //   字段缺失不抛异常，拿默认值——对手写/老版本文件宽容。
    //   "宽容读取，严格写出"（Postel 法则）
    // ========================================================
    static void parseInto(AnimLib &lib, const nlohmann::json &doc)
    {
        for (auto &[clipName, cj] : doc["clips"].items()) // 每首歌
        {
            AnimClip clip;
            clip.duration = cj.value("duration", 1.0f);
            for (auto &tj : cj.value("tracks", nlohmann::json::array())) // 每条音轨
            {
                AnimTrack tr;
                tr.comp = tj.value("comp", "");
                tr.field = tj.value("field", "");
                // ※kind 保持 kUnresolved：翻译在首次播放时进行
                for (auto &kj : tj.value("keys", nlohmann::json::array())) // 每个音符
                {
                    AnimKeyframe k;
                    k.t = kj.value("t", 0.0f);
                    k.easing = kj.value("e", 0); // 文件没写 "e" → 线性
                    if (kj.contains("bz") && kj["bz"].size() == 4)
                        for (int bi = 0; bi < 4; ++bi)
                            k.bz[bi] = kj["bz"][bi].get<float>();
                    auto &v = kj["v"];
                    k.v = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
                    tr.keys.push_back(k); // 文件里已按序存，直接 push
                }
                clip.tracks.push_back(std::move(tr));
            }
            // 事件轨道
            for (auto &ej : cj.value("events", nlohmann::json::array()))
                clip.addEvent(ej.value("t", 0.0f), ej.value("s", ""));
            lib.clips[clipName] = std::move(clip);
        }
    }

    std::map<std::string, AnimLib> m_cache; // 文件路径 → 动画库（核心缓存）
};

#endif
