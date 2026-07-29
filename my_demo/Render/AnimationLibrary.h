#ifndef ANIMATIONLIBRARY_H
#define ANIMATIONLIBRARY_H

#include "Core/Easing.h" // 缓动算法库（策略模式，关键帧插值可选曲线）

// flecs：反射写入需要（EcsStruct 元数据）。emit 宏保护：本头会被 Qt 代码 include
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cmath>

// ============================================================
// AnimationLibrary — 动画数据中心（编辑器与播放系统共享）
//
// 【为什么独立成模块】
//   动画数据有两个消费者，职责完全不同：
//     AnimationSystem（运行时）→ 只读采样
//     EditorPanel（编辑器）    → 读写：打关键帧/新建clip/保存文件
//   数据若藏在 System 私有缓存里，编辑器就做不了动画工具 →
//   抽出共享库，编辑/播放彻底解耦（对标 Godot Animation 资源独立于 AnimationPlayer）
//
// 【Property Track（对标 Godot）】
//   轨道目标不再硬编码 position/rotation/scale，而是反射路径：
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

struct AnimKeyframe
{
    float t;        // 时间（秒）
    glm::vec3 v;    // 值（标量字段只用 v.x，bool 用 v.x>0.5）
    int easing = 0; // 本帧→下一帧的缓动方式（EasingType，0=线性）
                    // Godot 同款 per-key transition
    // 自定义贝塞尔控制点（仅 easing==kCustomBezier 时使用）
    // 默认值 = CSS 的 "ease" 曲线
    float bz[4] = {0.25f, 0.1f, 0.25f, 1.0f};
};

struct AnimTrack
{
    // ★ Property Track：反射路径（组件名 + 字段名）
    std::string comp;  // 如 "TransformComponent"
    std::string field; // 如 "position"

    std::vector<AnimKeyframe> keys; // 始终按时间升序

    // ---- 运行时解析缓存（applyTrack 首次使用时填写，避免每帧字符串查找）----
    enum FieldKind
    {
        kUnresolved = 0, // 尚未解析
        kInvalid,        // 解析失败（组件/字段不存在），跳过不写
        kVec3,
        kFloat,
        kBool,
        kInt32,
    };
    FieldKind kind = kUnresolved;
    ecs_entity_t compId = 0; // 组件类型实体
    int32_t offset = 0;      // 字段在组件内的字节偏移

    /// 编辑器改了 comp/field 后调用（重新解析）
    void invalidate() { kind = kUnresolved; }

    /// 打关键帧：同一时刻（±10ms）已有帧则覆盖值，否则按时间序插入
    /// bz：自定义贝塞尔 4 参数（仅 easing==kCustomBezier 时有意义，可传 nullptr）
    void setKey(float t, const glm::vec3 &v, int easing = 0, const float *bz = nullptr)
    {
        for (auto &k : keys)
        {
            if (std::fabs(k.t - t) < 0.01f)
            {
                k.v = v; // 覆盖（Godot 同行为：同点重打=更新）
                k.easing = easing;
                if (bz)
                    for (int i = 0; i < 4; ++i)
                        k.bz[i] = bz[i];
                return;
            }
        }
        AnimKeyframe nk{t, v, easing};
        if (bz)
            for (int i = 0; i < 4; ++i)
                nk.bz[i] = bz[i];
        auto it = keys.begin();
        while (it != keys.end() && it->t < t)
            ++it;
        keys.insert(it, nk);
    }

    /// 删除指定下标的关键帧（编辑器列表删帧用）
    void removeKeyAt(int index)
    {
        if (index >= 0 && index < (int)keys.size())
            keys.erase(keys.begin() + index);
    }

    /// 轨道显示名（编辑器用）："组件名/字段名"
    std::string trackName() const { return comp + "/" + field; }

    /// 采样：线性插值。首帧前取首帧值，末帧后取末帧值（Godot 同行为）
    glm::vec3 sample(float time) const
    {
        if (keys.empty())
            return glm::vec3(0);
        if (time <= keys.front().t)
            return keys.front().v;
        if (time >= keys.back().t)
            return keys.back().v;
        for (size_t i = 1; i < keys.size(); ++i)
        {
            if (time < keys[i].t)
            {
                const AnimKeyframe &a = keys[i - 1];
                const AnimKeyframe &b = keys[i];
                float span = b.t - a.t;
                float alpha = span > 0 ? (time - a.t) / span : 0;
                // ★ 缓动：线性进度 → 曲线进度（算法由起始帧指定，策略模式）
                // 自定义贝塞尔 = 用户用 4 个参数定义的专属曲线（数据驱动）
                if (a.easing == (int)EasingType::kCustomBezier)
                    alpha = easeBezier(a.bz[0], a.bz[1], a.bz[2], a.bz[3], alpha);
                else
                    alpha = ease(a.easing, alpha);
                return a.v + (b.v - a.v) * alpha; // lerp
            }
        }
        return keys.back().v;
    }
};

struct AnimClip
{
    float duration = 1.0f;
    std::vector<AnimTrack> tracks;

    // ---- 事件轨道（Method Track，对标 Godot）----
    // 播放头跨越事件时刻 → 发射信号（走信号总线，Lua connect 接收）
    // 动画不直接调函数 —— 保持解耦：动画只负责"什么时候"，逻辑在脚本
    struct AnimEvent
    {
        float t;            // 触发时刻（秒）
        std::string signal; // 信号名（如 "attack_hit"、"play_sound"）
    };
    std::vector<AnimEvent> events; // 按 t 升序

    /// 添加事件帧（同时刻同名覆盖，按时间序插入）
    void addEvent(float t, const std::string &signal)
    {
        for (auto &ev : events)
            if (std::fabs(ev.t - t) < 0.01f && ev.signal == signal)
                return; // 已存在
        auto it = events.begin();
        while (it != events.end() && it->t < t)
            ++it;
        events.insert(it, {t, signal});
    }

    void removeEventAt(int index)
    {
        if (index >= 0 && index < (int)events.size())
            events.erase(events.begin() + index);
    }

    /// ★ 跨越检测：收集播放头从 prev 到 cur 之间跨过的事件
    ///
    /// 为什么不能用 time == 事件时刻：帧率离散，播放头几乎不会
    /// 正好落在 0.30s 上（可能从 0.29 直接跳到 0.31）→ 必须区间检测
    ///
    /// 三种情况：
    ///   正放：prev < cur     → 触发 (prev, cur]
    ///   回绕：prev > cur（loop 取模后）→ (prev, duration] + [0, cur]
    ///   倒放：speed<0 时 prev > cur 且非回绕 → 由调用方传 reversed 区分
    void collectEvents(float prev, float cur, bool wrapped,
                       std::vector<const AnimEvent *> &out) const
    {
        auto inRange = [](float t, float lo, float hi)
        { return t > lo && t <= hi; };
        for (const auto &ev : events)
        {
            bool hit = false;
            if (wrapped)
                hit = inRange(ev.t, prev, duration + 0.001f) || (ev.t >= 0 && ev.t <= cur);
            else if (cur >= prev)
                hit = inRange(ev.t, prev, cur);
            else // 倒放：区间反向
                hit = ev.t >= cur && ev.t < prev;
            if (hit)
                out.push_back(&ev);
        }
    }

    /// 找到（或创建）指定反射路径的轨道
    AnimTrack &track(const std::string &comp, const std::string &field)
    {
        for (auto &t : tracks)
            if (t.comp == comp && t.field == field)
                return t;
        AnimTrack t;
        t.comp = comp;
        t.field = field;
        tracks.push_back(std::move(t));
        return tracks.back();
    }
};

// ============================================================
// applyClip — 反射写入（Property Track 的核心）
//
// 对 clip 里每条轨道：采样 → 按反射路径找到实体组件字段 → 直接写内存。
// 运行时播放（AnimationSystem）和编辑器 scrub 预览共用同一个函数。
//
// 解析（首次）：
//   组件名 → ecs_lookup 找类型实体 → EcsStruct 元数据遍历找字段
//   → 记住 offset + 类型（缓存进轨道，后续帧零查找开销）
// 写入：组件数据指针 + offset → 按类型写 vec3/float/bool/int32
// ============================================================
inline void applyClip(flecs::world &w, ecs_entity_t entity, AnimClip &clip, float time)
{
    for (auto &tr : clip.tracks)
    {
        // ---- 惰性解析反射路径（一次解析，长期缓存）----
        if (tr.kind == AnimTrack::kUnresolved)
        {
            tr.kind = AnimTrack::kInvalid; // 先标失败，解析成功再覆盖
            tr.compId = ecs_lookup(w.c_ptr(), tr.comp.c_str());
            if (tr.compId)
            {
                const EcsStruct *st = (const EcsStruct *)ecs_get_id(
                    w.c_ptr(), tr.compId, ecs_id(EcsStruct));
                if (st)
                {
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
                            tr.kind = AnimTrack::kVec3; // 结构体字段：目前即 glm::vec3
                        break;
                    }
                }
            }
        }
        if (tr.kind == AnimTrack::kInvalid || tr.keys.empty())
            continue;

        // ---- 实体没挂该组件则跳过（不自动添加，动画不改变实体结构）----
        if (!ecs_has_id(w.c_ptr(), entity, tr.compId))
            continue;
        void *data = ecs_get_mut_id(w.c_ptr(), entity, tr.compId);
        if (!data)
            continue;

        glm::vec3 v = tr.sample(time);
        char *fieldPtr = (char *)data + tr.offset;
        switch (tr.kind)
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
}

struct AnimLib
{
    std::map<std::string, AnimClip> clips;
};

// ============================================================
// AnimationLibraryManager — 加载缓存 + 编辑 + 存盘（单例）
// ============================================================
class AnimationLibraryManager
{
public:
    static AnimationLibraryManager &instance()
    {
        static AnimationLibraryManager m;
        return m;
    }

    /// 取动画库（缓存加载）。文件不存在也返回空库——编辑器要能对新文件做动画
    AnimLib *getOrCreate(const std::string &path)
    {
        if (path.empty())
            return nullptr;
        auto it = m_cache.find(path);
        if (it != m_cache.end())
            return &it->second;

        AnimLib &lib = m_cache[path]; // 先占位（文件无效时 = 空库可编辑）
        std::ifstream in(path);
        if (in)
        {
            nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
            if (!doc.is_discarded() && doc.contains("clips"))
                parseInto(lib, doc);
        }
        return &lib;
    }

    /// 内存库写回 .anim 文件（编辑器「保存动画」按钮）
    bool save(const std::string &path)
    {
        auto it = m_cache.find(path);
        if (it == m_cache.end())
            return false;
        nlohmann::json doc;
        for (auto &[name, clip] : it->second.clips)
        {
            nlohmann::json cj;
            cj["duration"] = clip.duration;
            cj["tracks"] = nlohmann::json::array();
            for (auto &tr : clip.tracks)
            {
                nlohmann::json tj;
                tj["comp"] = tr.comp;   // 反射路径：组件名
                tj["field"] = tr.field; // 反射路径：字段名
                tj["keys"] = nlohmann::json::array();
                for (auto &k : tr.keys)
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
            return false;
        out << doc.dump(2);
        return true;
    }

private:
    AnimationLibraryManager() = default;

    static void parseInto(AnimLib &lib, const nlohmann::json &doc)
    {
        for (auto &[clipName, cj] : doc["clips"].items())
        {
            AnimClip clip;
            clip.duration = cj.value("duration", 1.0f);
            for (auto &tj : cj.value("tracks", nlohmann::json::array()))
            {
                AnimTrack tr;
                tr.comp = tj.value("comp", "");
                tr.field = tj.value("field", "");
                for (auto &kj : tj.value("keys", nlohmann::json::array()))
                {
                    AnimKeyframe k;
                    k.t = kj.value("t", 0.0f);
                    k.easing = kj.value("e", 0); // 旧文件无此字段 → 线性（向后兼容）
                    if (kj.contains("bz") && kj["bz"].size() == 4)
                        for (int bi = 0; bi < 4; ++bi)
                            k.bz[bi] = kj["bz"][bi].get<float>();
                    auto &v = kj["v"];
                    k.v = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
                    tr.keys.push_back(k);
                }
                clip.tracks.push_back(std::move(tr));
            }
            // 事件轨道
            for (auto &ej : cj.value("events", nlohmann::json::array()))
                clip.addEvent(ej.value("t", 0.0f), ej.value("s", ""));
            lib.clips[clipName] = std::move(clip);
        }
    }

    std::map<std::string, AnimLib> m_cache; // 路径 → 动画库
};

#endif
