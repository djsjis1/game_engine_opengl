#ifndef TWEENSYSTEM_H
#define TWEENSYSTEM_H

#include "Core/System.h"
#include <string>

// ============================================================
// TweenSystem — 属性补间（对标 Godot Tween/create_tween）
//
// 脚本一行平滑动画：
//   tween(id, "position", x,y,z, 0.5, "ease_out")          — 门滑开
//   tween(id, "rotation", 0,180,0, 1.0, "bounce")          — 招牌翻转
//   tween(id, "scale", 2,2,2, 0.3, "elastic", "grow_done") — 完成发信号
//
// 与 AnimationComponent 的分工：
//   动画 = 设计期资产（.anim 文件、多轨道、循环播放）
//   Tween = 运行期一次性过渡（脚本随手发起，播完即弃）——Godot 同分工
//
// 【实现】活动补间列在 TweenSystem.cpp 内部（匿名命名空间，文件私有）；
//   脚本经 tweenStart() 全局函数投递（同 physicsApplyImpulse 模式）。
//   同实体同属性的新 tween 顶掉旧的（Godot 行为：后发优先）。
// ============================================================
class TweenSystem : public System
{
public:
    int phase() const override { return 105; }           // 脚本(100)后：当帧发起当帧生效
    int activeIn() const override { return kAllStates; } // 队列空则零开销

    void update(flecs::world &world, float dt) override;

    /// 退出 Play 时清空全部补间（残留的 tween 会在下次 Play 乱跳）
    static void clearAll();
};

/// 脚本入口：启动一个补间（target: "position"/"rotation"/"scale"）
/// easing 见 Easing.h 枚举值；doneSignal 非空则完成时给实体发该信号
void tweenStart(flecs::entity_t id, const std::string &target,
                float x, float y, float z, float duration,
                int easing, const std::string &doneSignal);

#endif
