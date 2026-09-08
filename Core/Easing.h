#ifndef EASING_H
#define EASING_H

#include <cmath>

// ============================================================
// Easing — 缓动算法库（策略模式：一族可互换的插值算法）
//
// 【解耦设计】
//   纯数学函数，零外部依赖 —— 任何模块都能用：
//     动画关键帧插值 / 相机平滑 / UI 过渡 / 补间（tween）
//
//   输入输出契约：ease(type, t)
//     t ∈ [0,1]（归一化进度） → 返回 [0,1] 附近的缓动进度
//     所有函数满足 f(0)=0, f(1)=1（Elastic/Back 中途可越界，属正常）
//
// 【算法族】Robert Penner 缓动方程（行业标准，Godot/Unity/CSS 同款）
//   In    = 慢起步（加速）      Out   = 慢结束（减速）
//   InOut = 两头慢中间快        Bounce/Elastic = 弹跳/弹簧
//
// 【扩展方式】加新算法只需：枚举加一项 + switch 加一个 case + 名字表加一行
//   使用方（动画/编辑器）零改动 —— 开闭原则
// ============================================================

enum class EasingType : int
{
    kLinear = 0,
    kEaseInQuad,     // t²        慢进
    kEaseOutQuad,    //           慢出
    kEaseInOutQuad,  //           慢进慢出
    kEaseInCubic,    // t³        更陡的慢进
    kEaseOutCubic,   //           更陡的慢出
    kEaseInOutCubic, //           更陡的慢进慢出
    kEaseInSine,     // 正弦      最柔和
    kEaseOutSine,    //
    kEaseInOutSine,  //
    kEaseOutBounce,  // 落地弹跳（球落地）
    kEaseOutElastic, // 弹簧回弹（UI 弹出）
    kCustomBezier,   // ★ 用户自定义：cubic-bezier(x1,y1,x2,y2) 四参数曲线
    kCount           // 数量（编辑器遍历用）
};

/// 缓动函数名字表（编辑器下拉框显示，与枚举一一对应）
inline const char *easingName(int i)
{
    static const char *names[] = {
        "线性", "慢进(2次)", "慢出(2次)", "慢进慢出(2次)",
        "慢进(3次)", "慢出(3次)", "慢进慢出(3次)",
        "正弦进", "正弦出", "正弦进出",
        "弹跳落地", "弹簧回弹", "自定义贝塞尔"};
    return (i >= 0 && i < (int)EasingType::kCount) ? names[i] : "线性";
}

/// 核心：把线性进度 t(0..1) 变换为缓动进度
inline float ease(EasingType type, float t)
{
    constexpr float kPi = 3.14159265358979f;
    switch (type)
    {
    case EasingType::kLinear:
        return t;

    case EasingType::kEaseInQuad:
        return t * t;
    case EasingType::kEaseOutQuad:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case EasingType::kEaseInOutQuad:
        return t < 0.5f ? 2.0f * t * t
                        : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

    case EasingType::kEaseInCubic:
        return t * t * t;
    case EasingType::kEaseOutCubic:
        return 1.0f - std::pow(1.0f - t, 3.0f);
    case EasingType::kEaseInOutCubic:
        return t < 0.5f ? 4.0f * t * t * t
                        : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

    case EasingType::kEaseInSine:
        return 1.0f - std::cos(t * kPi / 2.0f);
    case EasingType::kEaseOutSine:
        return std::sin(t * kPi / 2.0f);
    case EasingType::kEaseInOutSine:
        return -(std::cos(kPi * t) - 1.0f) / 2.0f;

    case EasingType::kEaseOutBounce:
    {
        // 标准四段弹跳（Penner）
        constexpr float n1 = 7.5625f;
        constexpr float d1 = 2.75f;
        if (t < 1.0f / d1)
            return n1 * t * t;
        if (t < 2.0f / d1)
        {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        }
        if (t < 2.5f / d1)
        {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        }
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }

    case EasingType::kEaseOutElastic:
    {
        constexpr float c4 = 2.0f * kPi / 3.0f;
        if (t <= 0.0f)
            return 0.0f;
        if (t >= 1.0f)
            return 1.0f;
        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }

    default:
        return t;
    }
}

/// 便捷重载：int 类型（组件里存 int，避免枚举序列化问题）
inline float ease(int type, float t)
{
    if (type < 0 || type >= (int)EasingType::kCount)
        type = 0;
    return ease((EasingType)type, t);
}

// ============================================================
// cubic-bezier —— 用户自定义缓动（CSS transition 同款机制）
//
// 曲线由固定端点 P0=(0,0)、P3=(1,1) 和两个用户控制点
// P1=(x1,y1)、P2=(x2,y2) 定义。四个数字 = 一条自定义算法：
//   cubic-bezier(0.25, 0.1, 0.25, 1.0)  → CSS 的 "ease"
//   cubic-bezier(0.68, -0.55, 0.27, 1.55) → 过冲回弹（back）
//   y 坐标可超出 [0,1] → 制造过冲/预备动作
//
// 求解：给定时间进度 t（=曲线的 x），二分迭代求参数 u 使 x(u)=t，
// 再返回 y(u)。x 分量单调（x1,x2∈[0,1] 保证），二分必收敛。
// ============================================================
inline float easeBezier(float x1, float y1, float x2, float y2, float t)
{
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;

    // 二分求 u：x(u) = 3(1-u)²u·x1 + 3(1-u)u²·x2 + u³ = t
    float lo = 0.0f, hi = 1.0f, u = t;
    for (int i = 0; i < 24; ++i) // 24 次二分 ≈ 1e-7 精度，足够
    {
        float inv = 1.0f - u;
        float x = 3.0f * inv * inv * u * x1 + 3.0f * inv * u * u * x2 + u * u * u;
        if (x < t)
            lo = u;
        else
            hi = u;
        u = (lo + hi) * 0.5f;
    }
    float inv = 1.0f - u;
    return 3.0f * inv * inv * u * y1 + 3.0f * inv * u * u * y2 + u * u * u;
}

#endif
