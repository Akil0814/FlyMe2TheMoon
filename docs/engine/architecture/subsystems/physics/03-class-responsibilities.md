# 物理组件的职责

物理模块通过世界、参与者、定义和事件分开表达运行状态与游戏接入。下表用于定位修改应该发生在哪一层。

| 组件 | 职责 |
| --- | --- |
| `PhysicsWorld` | 持有世界，管理句柄、命令、模拟步、查询和事件 |
| `PhysicsParticipant` | 向场景提供创建定义，接收世界与句柄绑定 |
| `PhysicsStepParticipant` | 每个模拟步中的游戏控制逻辑 |
| `BodyDefinition` / `BodyState` | 创建时的输入定义 / 已提交模拟状态的只读输出 |
| `Collider` | 可复制的形状、过滤、材质和密度描述 |
| `PhysicsUnits` | 统一的空间单位换算边界 |
| `ContactCache` | 引擎的 Begin/Stay/End 状态，包括逻辑失效 |
| `GameplayCollisionRuntime` | 队伍与角色路由、攻击实例去重 |
| `ITileCollisionWorld` | 借用的地图描述及显式脏区域刷新 |
| `JointHandle` | 八种关节的引擎身份 |
| `ChainDefinition` / `ChainHandle` | 复制地形顶点、ghost 与标签 / 管理整组边段生命周期 |
| `PhysicsWorld::set_target_transform` | 按执行时姿态提交一次运动学目标速度 |

例如，改变角色如何移动，应修改控制逻辑或提交物理命令；改变一次攻击如何判定目标，应检查玩法角色、过滤和显式查询。已移除的检测与求解策略不是扩展入口。
