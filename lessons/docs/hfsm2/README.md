# HFSM2 层次状态机学习课程

> 从零学习 HFSM2（Hierarchical Finite State Machine 2）C++ 状态机库。
> 纯框架知识，不绑定任何具体项目。基于 HFSM2 2.x，header-only。

---

## 📚 阅读顺序

| 序号 | 文档 | 核心内容 | 预计时间 |
|------|------|----------|----------|
| **01** | [状态机基础与入门](./01-状态机基础与入门.md) | 什么是 FSM、HFSM2 安装、第一个状态机、状态切换 | 15 分钟 |
| **02** | [层次状态机](./02-层次状态机.md) | 父/子状态、状态嵌套、进入/退出传播 | 15 分钟 |
| **03** | [Context 与数据共享](./03-Context与数据共享.md) | ContextT、状态间通信、引用语义 | 10 分钟 |
| **04** | [转换、守卫与动作](./04-转换守卫与动作.md) | changeTo、react、enter/exit、update 回调 | 15 分钟 |
| **05** | [高级特性](./05-高级特性.md) | 正交区域、历史状态、isActive、序列化 | 15 分钟 |

---

## 🎯 学完后你能做什么

- 用 HFSM2 实现游戏状态管理（菜单/游戏/暂停）
- 实现 AI 行为状态机（巡逻/追击/攻击/逃跑）
- 实现编辑器模式切换（编辑/运行/调试）
- 理解层次状态机与扁平状态机的区别
- 看懂 HFSM2 官方文档和示例

---

## 📖 延伸学习

| 资源 | 链接 |
|------|------|
| HFSM2 GitHub | <https://github.com/andrew-gresyk/HFSM2> |
| HFSM2 文档 | <https://github.com/andrew-gresyk/HFSM2/wiki> |
| HFSM2 示例 | <https://github.com/andrew-gresyk/HFSM2/tree/master/examples> |
| 状态机模式（Game Programming Patterns） | <https://gameprogrammingpatterns.com/state.html> |
