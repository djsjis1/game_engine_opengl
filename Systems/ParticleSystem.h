#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

#include "Core/System.h"
#include "Components/ParticleComponent.h"
#include "Components/TransformComponent.h"
#include <cstdlib>
#include <cmath>

// ============================================================
// ParticleSystem — 粒子 CPU 模拟（发射 → 积分 → 回收）
//
// 对标 Godot CPUParticles3D 的处理循环。纯逻辑无 GL——渲染由
// RenderWidget 读组件粒子池做实例化绘制（数据与呈现分离，可单测）。
//
// 【每帧流程】对每个 (Transform + Particle) 实体：
//   ① 发射：emitAccum += rate*dt，攒够 1.0 生成一个粒子
//      （初速度在 direction 锥内随机，位置=发射器世界位置）
//   ② 积分：vel += gravity*dt; pos += vel*dt; age += dt
//   ③ 回收：age >= life 的与末尾交换后 pop（O(1) 删除，顺序无关紧要）
//
// 【编辑/运行都跑】（kAllStates）：编辑器里挂上组件立即预览效果，
//   Godot 编辑器同行为——调参数即所见即所得。
// ============================================================
class ParticleSystem : public System
{
public:
    int phase() const override { return 110; }           // 脚本(100)之后：本帧 emitting 开关立即生效
    int activeIn() const override { return kAllStates; } // 编辑模式也预览

    void update(flecs::world &world, float dt) override
    {
        if (!isEnabled() || dt <= 0.0f)
            return;

        world.each([&](flecs::entity, TransformComponent &tf, ParticleComponent &pc)
                   {
            // ① 发射（发射器世界位置 = worldMatrix 平移列，含父子层级）
            if (pc.emitting && pc.rate > 0.0f)
            {
                pc.emitAccum += pc.rate * dt;
                glm::vec3 origin(tf.worldMatrix[3]);
                while (pc.emitAccum >= 1.0f &&
                       (int)pc.pool.size() < pc.maxParticles)
                {
                    pc.emitAccum -= 1.0f;
                    pc.pool.push_back(spawn(pc, origin));
                }
                // 池满时丢弃多余配额（不然满池期间攒一大坨，腾出空间瞬间爆喷）
                if (pc.emitAccum >= 1.0f)
                    pc.emitAccum = 0.0f;
            }

            // ② 积分 + ③ 回收（swap-and-pop：粒子无序，删除 O(1)）
            for (size_t i = 0; i < pc.pool.size();)
            {
                Particle &p = pc.pool[i];
                p.age += dt;
                if (p.age >= p.life)
                {
                    p = pc.pool.back();
                    pc.pool.pop_back();
                    continue; // 换过来的粒子下轮循环处理（i 不动）
                }
                p.vel += pc.gravity * dt;
                p.pos += p.vel * dt;
                ++i;
            } });
    }

private:
    /// 生成一个新粒子：方向在 direction 为轴、spread 为半角的锥内均匀随机
    static Particle spawn(const ParticleComponent &pc, const glm::vec3 &origin)
    {
        Particle p;
        p.pos = origin;
        p.life = pc.lifetime > 0.01f ? pc.lifetime : 0.01f;

        // 锥内随机方向：以 +Z 为轴生成，再旋转对齐 direction
        float spreadRad = glm::radians(pc.spread);
        float cosSpread = std::cos(spreadRad);
        float u = frand();                              // [0,1]
        float cosTheta = 1.0f - u * (1.0f - cosSpread); // 锥内均匀
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
        float phi = frand() * 6.2831853f;
        glm::vec3 local(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

        // +Z → direction 的旋转（Rodrigues；共线时退化处理）
        glm::vec3 dir = glm::normalize(
            glm::dot(pc.direction, pc.direction) > 1e-6f ? pc.direction
                                                         : glm::vec3(0, 1, 0));
        glm::vec3 z(0, 0, 1);
        float d = glm::dot(z, dir);
        glm::vec3 world;
        if (d > 0.999f)
            world = local; // 同向：不用转
        else if (d < -0.999f)
            world = -local; // 反向：翻转
        else
        {
            glm::vec3 axis = glm::normalize(glm::cross(z, dir));
            float ang = std::acos(d);
            float c = std::cos(ang), s = std::sin(ang);
            world = local * c + glm::cross(axis, local) * s +
                    axis * glm::dot(axis, local) * (1.0f - c);
        }
        p.vel = world * pc.speed;
        return p;
    }

    /// [0,1] 均匀随机（粒子效果不需要高质量随机，rand 足够且零依赖）
    static float frand() { return (float)std::rand() / (float)RAND_MAX; }
};

#endif
