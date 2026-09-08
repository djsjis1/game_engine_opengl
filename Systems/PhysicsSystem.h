#ifndef PHYSICSSYSTEM_H
#define PHYSICSSYSTEM_H

#include "Core/System.h"

// ============================================================
// PhysicsSystem — Jolt Physics 封装（ECS System）
//
// 职责：
//   1. init()  — 创建 Jolt 物理世界（重力、碰撞层）
//   2. update()— 每帧固定步长模拟：
//        a. 首次遇到 (Rigidbody + BoxCollider + Transform) 实体
//           → 在 Jolt 世界创建刚体
//        b. Kinematic/Static 实体：Transform → Jolt（脚本推动物理体）
//        c. 物理模拟 step
//        d. Dynamic 实体：Jolt → Transform（物理结果写回渲染）
//   3. shutdown() — 销毁 Jolt 世界
//
// pImpl 模式：Jolt 头文件只在 .cpp 中，编译隔离。
// ============================================================
class PhysicsSystem : public System
{
public:
    void init();
    void shutdown() override;
    void update(flecs::world &world, float dt) override;

    /// 销毁全部 Jolt 刹体（停止 Play 时调用，下次 Play 从当前 Transform 重建）
    void resetBodies();

    int phase() const override { return 90; }                        // Script(100) 之前
    int activeIn() const override { return (int)EngineState::Play; } // 只在运行时模拟

    struct Impl; // 公开前置声明（定义在 .cpp，供脚本物理 API 全局指针使用）

private:
    Impl *m_impl = nullptr; // pImpl：隔离 Jolt 头文件
};

// ============================================================
// 脚本物理 API（LuaApi.cpp 调用，实现在 PhysicsEngine.cpp）
// 内部通过全局 Impl 指针访问当前物理世界
// ============================================================

/// 给实体施加冲量（瞬间推力，如跳跃）
bool physicsApplyImpulse(flecs::entity_t id, float x, float y, float z);

/// 直接设置实体速度
bool physicsSetVelocity(flecs::entity_t id, float x, float y, float z);

/// 读取刚体线速度（移动时保留 y 速度用）；无刚体返回 false
bool physicsGetVelocity(flecs::entity_t id, float &x, float &y, float &z);

/// 角色控制器（对标 Godot CharacterBody3D）：
/// 水平移动自动保留竖直速度（重力/跳跃不被抹掉）
bool physicsMoveSlide(flecs::entity_t id, float vx, float vz);

/// 落地判定（向下短射线，自动忽略自己、自动取碰撞体半高）
bool physicsIsOnFloor(flecs::entity_t id);

/// 传送刚体（读档/重生用）：只改 Transform 会被物理回写拉回原地
bool physicsSetPosition(flecs::entity_t id, float x, float y, float z);

/// 玩法射线（脚本 raycast 调用）：命中返回 true 并填写命中实体/点/距离
/// 只检测物理碰撞体（Jolt BVH 加速）；编辑器拾取另走 Core/Raycast.h
/// ignoreEntity：忽略该实体的碰撞体（发射者自己——否则从自身中心发射
/// 会命中自己；Godot intersect_ray 的 exclude 同款语义）
bool physicsRaycast(float ox, float oy, float oz,
                    float dx, float dy, float dz, float maxDist,
                    flecs::entity_t &hitEntity,
                    float &hitX, float &hitY, float &hitZ, float &hitDist,
                    flecs::entity_t ignoreEntity = 0);

#endif
