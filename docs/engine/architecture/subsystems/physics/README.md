# 物理系统

物理系统使用 Box2D 3.1.1 完成模拟与空间查询。游戏层使用 Elysia 的句柄、定义、命令和事件；Box2D 的对象标识与头文件属于内部实现。

本目录面向维护物理模块的开发者。游戏接入示例见[物理使用指南](../../../user-guide/systems/physics.md)，公开能力的集中说明见[功能与接口总览](../../../reference/physics/README.md)。

## 按问题阅读

- [单位与公开接口](../../../reference/physics/units-and-state.md)：世界单位、命令、状态快照与调试绘制。
- [架构与所有权](02-target-architecture.md)、[组件职责](03-class-responsibilities.md)：谁持有模拟状态，场景与适配层如何分工。
- [生命周期与限制](01-current-state-audit.md)、[固定步流程](04-function-responsibilities.md)：句柄失效、延迟命令、事件提取和回调顺序。
- [碰撞与查询](05-collision-algorithms.md)、[地图碰撞](06-tile-map-collision.md)：过滤、连续查询、单向平台和地图更新。
- [Gameplay 碰撞事件](07-gameplay-collision-runtime.md)：物理接触如何转换为玩法事件。
- [历史验证与性能](08-implementation-roadmap-and-tests.md)：迁移验收范围、微基准结果与复现入口。

## 实现边界

旧的积分器、宽阶段检测、检测策略和冲量求解器已移除，当前没有可选后端或求解策略注入接口。Gameplay 扩展应通过控制、过滤、单向规则和显式查询接入。

物理测试中心包含胶囊斜坡、八边形落体、四种形状扫掠、关节与原有回归场景。使用运行按钮启动，P 暂停、N 单步、R 重置、Esc 返回；关节页的控制按钮进入持续手动模拟。展示仅帮助观察，不代替自动化测试或实际手感验收。

第二阶段加入双面 Segment、开放／闭合 Chain、每固定步运动学目标和 Weld／Prismatic／Wheel／Mouse／Motor／Filter。测试中心新增 `edges`、`target_pose` 与六个 `joint_*` 控制页；Mouse 拖动目标，Filter 虚线标记配对过滤。
