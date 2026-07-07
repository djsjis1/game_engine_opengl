#ifndef SYSTEM_H
#define SYSTEM_H

// Qt 的 emit 宏会与 Flecs 的 emit 变量冲突，
// 包含 flecs.h 前必须先禁用 emit，包含完再恢复。
#pragma push_macro("emit")
#undef emit
#include <flecs.h>
#pragma pop_macro("emit")

// ============================================================
// System —— 所有系统的基类
//
// 【是什么】
//   系统 = "每帧执行一次的 for 循环"
//   它从 ECS 世界查询特定组件组合，然后做一件事。
//
// 【怎么用】
//   继承这个类，实现 update(world, dt)。
//   在 update 里用 world.each<T1, T2>(lambda) 遍历实体。
//
//   class RotationSystem : public System {
//       void update(flecs::world &world, float dt) override {
//           world.each([dt](TransformComponent &tf, const RotateBehavior &rb) {
//               tf.rotation.y += rb.speedY * dt;
//           });
//       }
//   };
//
// 【设计原则】
//   一个 System 只做一件事（单一职责）。
//   System 之间不直接通信——通过 ECS World 里的组件数据间接交互。
// ============================================================
class System
{
public:
    virtual ~System() = default;

    // 每帧由 Engine::runFrame 调用
    // @param world  ECS 世界引用（读写实体和组件）
    // @param dt    本帧间隔（秒），变化值，不固定
    virtual void update(flecs::world &world, float dt) = 0;
};

#endif
