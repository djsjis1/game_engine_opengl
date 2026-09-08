#ifndef PARTICLECOMPONENT_H
#define PARTICLECOMPONENT_H

#include <glm/glm.hpp>
#include <vector>
#include "Components/Reflect.h"

// ============================================================
// ParticleComponent — 粒子发射器（对标 Godot CPUParticles3D）
//
// 【怎么用】实体挂本组件即成为发射器：粒子从实体位置喷出，
//   火焰/烟/爆炸/拾取特效全靠调参数（编辑器 Inspector 直接调，
//   REFLECT 全家桶：面板/序列化/动画轨道自动支持）。
//
// 【架构分工】（CPU 模拟 + GPU 实例化，OpenGL 3.3 无 compute shader，
//   Godot CPUParticles3D 同思路）：
//   ParticleSystem   — 每帧发射/积分/回收（纯 CPU 逻辑，可无 GL 测试）
//   RenderWidget     — 把粒子池上传 instance buffer，一次 instanced draw
//
// 【粒子在世界空间模拟】发射后不再跟随发射器——发射器移动会留轨迹
//   （火把挥舞拖尾），这是标准做法（Godot local_coords=false 默认）。
// ============================================================

/// 单个活粒子（运行时数据，ParticleSystem 写，RenderWidget 读）
struct Particle
{
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float age = 0.0f;  // 已存活秒数
    float life = 1.0f; // 寿命（秒），age>=life 回收
};

struct ParticleComponent
{
    // ---- 发射 ----
    REFLECT bool emitting = true;   // 开关（脚本 particles_emit 控制）
    REFLECT float rate = 20.0f;     // 每秒发射个数
    REFLECT int maxParticles = 200; // 池上限（防失控）
    REFLECT float lifetime = 1.5f;  // 单粒子寿命（秒）

    // ---- 初速度：direction 锥形内随机（spread=锥半角，度）----
    REFLECT glm::vec3 direction{0.0f, 1.0f, 0.0f};
    REFLECT float spread = 30.0f;
    REFLECT float speed = 3.0f;

    // ---- 受力 ----
    REFLECT glm::vec3 gravity{0.0f, -3.0f, 0.0f}; // 粒子专用重力（烟用正 y 上飘）

    // ---- 外观：颜色/大小随生命插值（start → end）----
    REFLECT glm::vec3 colorStart{1.0f, 0.8f, 0.2f}; // 默认火焰：亮黄
    REFLECT glm::vec3 colorEnd{1.0f, 0.15f, 0.0f};  //           → 暗红
    REFLECT float sizeStart = 0.15f;
    REFLECT float sizeEnd = 0.02f;
    REFLECT float intensity = 1.0f; // 颜色亮度倍率（>1 在 HDR 管线里发光）
    REFLECT int additive = 1;       // 混合：1=加法（火/光效） 0=普通 alpha（烟/尘）

    // ---- 运行时（非 REFLECT：不序列化不进编辑器，System 私管）----
    std::vector<Particle> pool; // 活粒子池
    float emitAccum = 0.0f;     // 发射累积器（rate*dt 攒够 1 发一个）
};

#endif
