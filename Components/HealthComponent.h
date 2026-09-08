#ifndef HEALTHCOMPONENT_H
#define HEALTHCOMPONENT_H

#include "Components/Reflect.h"

// ============================================================
// HealthComponent — 生命值（mini-UHT 自动化链路的验证组件）
//
// 注意看这个文件多干净：只有数据 + REFLECT 标记。
// 反射注册/编辑器UI/场景存档/Prefab/Undo快照 —— 全部自动获得：
//   reflect_gen 扫描 REFLECT → 生成注册代码 → 编辑器和序列化读同一份元数据
// ============================================================
struct HealthComponent
{
    REFLECT float hp = 100.0f;    // 当前生命值
    REFLECT float maxHp = 100.0f; // 生命上限
    REFLECT float regen = 1.0f;   // 每秒回复速度

    // 运行时字段示例：不打标记 = 编辑器/存档都不可见
    float damageFlash = 0.0f; // 受击闪白计时（游戏逻辑用）
};

#endif
