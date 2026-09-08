# Flecs ECS 学习课程

> 从零学习 Flecs ECS 框架。纯框架知识，不绑定任何具体项目。
> 基于 Flecs 4.x C++ API，每课含独立可运行的代码示例。

---

## 📚 阅读顺序

| 序号 | 文档 | 核心内容 | 预计时间 |
|------|------|----------|----------|
| **01** | [ECS 概念与 Flecs 基础](./01-ECS概念与flecs基础.md) | 什么是 ECS、World/Entity/Component、第一个程序 | 15 分钟 |
| **02** | [组件与实体操作](./02-组件与实体操作.md) | 定义组件、set/get/add/remove/has、实体生命周期 | 20 分钟 |
| **03** | [系统与查询](./03-系统与查询.md) | system、query、each、filter、可选组件、排除 | 20 分钟 |
| **04** | [关系与层级](./04-关系与层级.md) | Relationship、ChildOf、父子层级、树遍历 | 15 分钟 |
| **05** | [Observer 与钩子](./05-Observer与钩子.md) | OnAdd/OnRemove/OnSet、钩子函数、资源管理模式 | 15 分钟 |
| **06** | [反射与序列化](./06-反射与序列化.md) | member 注册、EcsStruct、JSON 序列化/反序列化 | 20 分钟 |
| **07** | [Pipeline 与高级特性](./07-Pipeline与高级特性.md) | Pipeline、Phase、单例组件、Tag、配对查询 | 20 分钟 |

---

## 🎯 学完后你能做什么

- 独立使用 Flecs 搭建 ECS 架构
- 理解 archetype 存储模型与查询性能
- 使用关系/层级构建场景图
- 使用反射实现自动序列化/编辑器面板
- 使用 Observer 实现资源自动管理
- 看懂 Flecs 官方文档和示例

---

## 📖 延伸学习

| 资源 | 链接 |
|------|------|
| Flecs 官方文档 | <https://www.flecs.dev/flecs/> |
| Flecs GitHub | <https://github.com/SanderMertens/flecs> |
| Flecs 示例集（C++） | <https://github.com/SanderMertens/flecs/tree/master/examples/cpp> |
| ECS FAQ | <https://github.com/SanderMertens/ecs-faq> |
| Flecs Discord | <https://discord.gg/BEzP5Rgrrp> |
