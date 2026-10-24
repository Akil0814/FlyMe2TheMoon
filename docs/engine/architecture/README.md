# 引擎架构与模块设计

本目录面向希望理解、维护或扩展引擎的开发者。各专题解释当前源码中的模块职责、对象所有权和运行流程；游戏层调用示例请从[使用指南](../user-guide/README.md)进入。

## 场景与游戏运行

- [场景生命周期](subsystems/scene-lifecycle.md)：场景缓存、进入与退出、重置、安全修改阶段，以及异常恢复和应用关闭顺序。
- [输入与控制](subsystems/input/README.md)：设备状态如何经过 UI 和玩家路由，最终成为固定步中的控制命令。
- [物理系统](subsystems/physics/README.md)：Box2D 集成、刚体与关节、碰撞事件、空间查询和地图碰撞。
- [相机](subsystems/camera.md)：相机槽位、跟随与演出、场景拥有的操作权限，以及最终呈现视角。

## 界面、资源与持久化

- [UI 架构](subsystems/ui/README.md#element-tree)：元素树、布局、输入、窗口表面和主题的内部协作。
- [资源加载管线](subsystems/resources/README.md)：资源请求组装、加载顺序和服务之间的职责。
- [运行时配置](subsystems/runtime-config.md)：启动默认值、用户设置和只读游戏配置的职责与提交边界。
- [音频架构](subsystems/audio.md)：资源持有、请求调度、轨道后端与渐变流程；公开调用规则见[音频参考](../reference/audio.md)。
- [存档服务](subsystems/save.md)：类型化数据、可靠写入、恢复和失败诊断。

## 开发工具与验收记录

- [开发演示](subsystems/development-demos.md)：Demo Gallery 和游戏侧演示与引擎能力的边界。
- [开发覆盖层](subsystems/development-overlay.md)：可选 Dear ImGui 接入、输入捕获和开发面板生命周期。
- [展示重构验收记录](subsystems/showcase-validation.md)、[相机展示验收记录](subsystems/camera-showcase-validation.md)：特定日期与环境下的历史结果，不代表当前工作树的测试状态。
