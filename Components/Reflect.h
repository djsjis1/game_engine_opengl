#ifndef REFLECT_H
#define REFLECT_H

// ============================================================
// REFLECT — 反射字段标记宏（对标 Unreal 的 UPROPERTY()）
//
// 用法：字段声明前加 REFLECT 前缀：
//   struct HealthComponent {
//       REFLECT float hp = 100.0f;  // 进反射：编辑器/存档/Prefab 自动支持
//       float damageFlash = 0.0f;   // 不标记：运行时字段，反射不可见
//   };
//
// 【原理】编译器视角：REFLECT 展开为空 → 零开销，代码照常编译
//        reflect_gen 视角：扫描到 REFLECT 前缀 → 生成 member 注册代码
//
// 【为什么用宏而不是注释标记】
//   1. 语义进代码：注释恢复"无语义"本分，随便写不会误触发
//   2. 编译器兜底：宏名拼错 → 编译报错（注释拼错会静默失效，字段悄悄消失）
//
// 支持的字段类型：float / bool / int / int32_t / glm::vec3
// ============================================================
#define REFLECT

#endif
