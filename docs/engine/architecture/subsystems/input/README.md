# Elysia Engine Input

输入子系统支持键盘分区、独立鼠标、多设备并行、玩家归属、公共 UI 协调和固定步命令交付。控制器由游戏显式创建；单机需要游戏会话来限定生命周期，不需要网络会话。

```text
SDL → InputSystem::snapshot()
    → SceneInputRouter：独立 UI → 消费／捕获 → 快捷操作 → 键盘分区与设备分流
    → LocalPlayerController：游戏动作映射
    → ControllerManager：缓存 → 实际固定 tick → ControlCommandReceiver

自定义 Controller：固定 tick 产生意图 → 同一 Manager 缓存与交付
```

- [架构与生命周期](architecture.md)：Service、Manager、上下文和作用域。
- [目录与依赖](architecture.md#目录与依赖)：基础输入、UI 输入、场景路由、引擎控制器和游戏控制代码的归属。
- [动作映射](../../../reference/input/action-mapping.md)：状态、事件、鼠标增量与映射切换。
- [控制器与命令](../../../reference/input/controllers.md)：游戏 API、自定义来源与角色契约。
- [场景集成](../../../user-guide/gameplay/scene.md)：会话、显式绑定、固定步与示例。
- [接口迁移说明](migration.md)：破坏性 API 变更与结果处理。
- [测试与调试](testing-and-debugging.md)：自动化覆盖与硬件验收。

当前输入实现已替换旧场景输入控制接口，不提供兼容别名、旧入口转发或双路广播。外部项目需要迁移。AI 决策、网络传输、个人焦点树、分屏及输入配置持久化不在当前范围。
