#ifndef TIME_H
#define TIME_H

#include <cstdint>
#include <algorithm>

// ============================================================
// Time —— Godot 式三时钟系统
//
// Update   (updateFPS=60) — 逻辑帧，可变步长 delta
// Physics  (physicsFPS=60) — 固定步长 physicsStep，落后时追赶
// Draw     (drawFPS=0) — 渲染帧，从 physics 插值保证不抖动
//
// 用法： 
//   Time::instance().step(rawDt);          // 每帧一次
//   float dt = Time::instance().delta;     // 逻辑用
//   while (Time::instance().consumePhysicsStep()) { /* 物理 */ }
//   float t = Time::instance().interp();   // 渲染插值
// ============================================================
struct Time
{
    // ---- 频率配置 ----
    static constexpr float kUpdateFPS = 60.f;  // 逻辑帧率
    static constexpr float kPhysicsFPS = 60.f; // 物理固定步长
    static constexpr float kDrawFPS = 0.f;     // 渲染帧率（0=不限）

    // ---- 每帧数据 ----
    float delta = 0.f;        // Update dt（可变，当前帧到上一帧）
    float totalTime = 0.f;    // 游戏运行总时间
    float physicsDelta = 0.f; // 固定步长 = 1/physicsFPS
    uint64_t frameCount = 0;
    uint64_t physicsTickCount = 0; // 累计物理步数

    // ---- 内部 ----
    float accumulator = 0.f;
    float physicsStep = 1.f / kPhysicsFPS;
    int physicsTodo = 0; // 本帧还需跑几步

    static Time &instance()
    {
        static Time t;
        return t;
    }

    /// 每帧调用一次。rawDt 必须限制上限防止螺旋死亡。
    void step(float rawDt)
    {
        if (rawDt > 0.25f)
            rawDt = 0.25f; // 螺旋死亡保护
        delta = rawDt;
        totalTime += rawDt;
        frameCount++;

        physicsDelta = physicsStep;

        // 固定步长追赶
        accumulator += rawDt;
        physicsTodo = 0;
        while (accumulator >= physicsStep)
        {
            accumulator -= physicsStep;
            physicsTodo++;
            physicsTickCount++;
        }
    }

    /// 物理：每步 return true，步数用完后 return false
    bool consumePhysicsStep()
    {
        if (physicsTodo <= 0)
            return false;
        physicsTodo--;
        return true;
    }

    /// 渲染插值系数 [0, 1)：两个物理步之间的位置
    float interp() const
    {
        return accumulator / physicsStep;
    }

    /// 平均帧率（每秒调用一次以求准确，此处简化）
    float averageFPS() const
    {
        return delta > 0.f ? 1.f / delta : 0.f;
    }
};

// ---- 便捷的差值补偿函数（两个 glm::vec3 / mat4 之间线性插值） ----
template <typename T>
T lerp(const T &a, const T &b, float t) { return a + (b - a) * t; }

#endif
