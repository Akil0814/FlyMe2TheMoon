# Elysia Engine 使用指南

本指南面向使用 Elysia Engine 开发游戏的开发者，提供从环境配置、构建运行到游戏内容开发的文档入口。

## 开始使用

从[构建、运行与测试](build-and-run.md)开始，了解工具链要求、构建命令、运行目录、测试方式与常见问题。

## 开发前必读

在开始开发游戏内容前，建议阅读以下文档：

- [核心概念](core_concepts.md)：了解场景、游戏对象与 UI 的职责，以及对象生命周期、更新和销毁时机。

## 从零构建游戏

如果需要从头创建一个基于 Elysia Engine 的游戏，请阅读[游戏初始化与集成指南](game-initialization.md)，了解 `IGameModule` 的职责、游戏入口的组织方式，以及如何注册和初始化游戏内容。

如果只是为已有游戏添加角色、武器、UI 或其他功能，可以跳过本节，直接查阅对应的开发任务文档。

## 游戏玩法相关功能

Elysia Engine 提供游戏玩法相关的基础模块，用于组织游戏场景、角色控制及游戏碰撞逻辑。

| 模块 | 用途 | 使用文档 |
| --- | --- | --- |
| Gameplay Collision | 游戏玩法碰撞、碰撞事件与关系处理 | [游戏玩法碰撞](gameplay/collision.md) |
| Gameplay Control | 控制器管理、控制命令与场景控制上下文 | [控制器与控制命令](gameplay/control.md) |
| `GameplayScene` | 组织需要角色控制或玩法碰撞的关卡 | [GameplayScene 使用指南](gameplay/scene.md) |

需要串起完整战斗流程时，阅读[从攻击命中到接收伤害](gameplay/damage.md)。这是游戏层组合示例：引擎提供命中事件，生命值、伤害计算和死亡规则由游戏实现。

指南中的 C++ 示例遵循[编码规范](../contributing/coding-standard.md)：私有成员使用 `_name_name`，命名常量使用 `kPascalCase`，普通变量优先写明类型。局部片段会说明所在阶段与前提；完整示例的编译检查方法见[构建指南](build-and-run.md#运行测试)。

## 按开发任务查阅

以下文档按照开发任务分类，可根据需要选择阅读。

| 开发任务 | 文档入口 |
| --- | --- |
| 创建场景、管理对象、暂停与切换 | [场景系统](scene/scene.md) |
| 编写对象行为、选择绘制层级与提交绘制命令 | [游戏对象与绘制](scene/game-objects.md) |
| 注册内容、配置预加载与资源清单 | [资源加载与使用](content/resources.md) |
| 配置输入设备、动作映射与场景输入 | [输入系统](systems/input.md) |
| 创建 UI 窗口、组织布局与组合控件 | [UI 使用指南](ui/README.md#create-window) |
| 显示世界名字牌、血条与气泡 | [世界空间 UI](gameplay/world-ui.md) |
| 接入地图碰撞、刷新单元格和穿平台 | [地图碰撞](systems/tilemap-collision.md) |
| 启用开发覆盖层并注册面板 | [开发面板](tools/development-panels.md) |
| 查询 UI 控件的公开接口 | [UI API 参考](../reference/ui/README.md) |
| 使用刚体、物理碰撞事件、空间查询与 TileMap 碰撞 | [物理接入指南](systems/physics.md) |
| 实现一方攻击、另一方受伤与死亡 | [从攻击命中到接收伤害](gameplay/damage.md) |
| 配置动画资源、图集与特效 | [动画与特效](systems/animation-and-effects.md) |
| 播放音乐与音效，控制音量和播放调度 | [音频服务](systems/audio.md) |
| 使用相机跟随、震屏与坐标转换 | [相机工作流](systems/camera.md) |
| 管理启动配置、用户设置与游戏配置 | [运行时配置](content/configuration.md) |
| 读写存档、处理恢复与错误 | [存档服务](content/save.md) |
| 使用时间缩放、固定步更新与计时回调 | [时间与计时器](systems/time-and-timers.md) |
| 查询翻译、切换语言与生成文本纹理 | [本地化与文本](systems/localization.md) |
| 按类型、绘制层级与空间条件查找游戏对象 | [游戏对象查询](systems/object-query.md) |
| 绘制调试图元与管理调试类别 | [调试绘制](tools/debug-draw.md) |
| 使用向量、矩形、颜色、随机数与日志 | [常用基础工具](tools/utilities.md) |

## 常用宏与服务入口

Elysia Engine 为部分常用服务提供便利宏，方便游戏开发者访问相关功能。

以下列出常用宏与服务访问入口及其用途、相关文档和接口定义文件。具体使用方式请参考对应文档或头文件。

| 入口 | 用途 | 介绍文档 | 接口定义 |
| --- | --- | --- | --- |
| `ELYSIA_ANIMATIONS` | 动画服务 | [动画与特效](systems/animation-and-effects.md) | [animation_service.h](../../../engine/animation/animation_service.h) |
| `ELYSIA_AUDIO` | 音频服务 | [音频服务](systems/audio.md) | [audio_service.h](../../../engine/audio/audio_service.h) |
| `ELYSIA_CONFIG` | 按类型读取游戏配置 | [运行时配置](content/configuration.md) | [config_service.h](../../../engine/config/config_service.h) |
| `ELYSIA_USER_CONFIG` | 用户设置管理 | [运行时配置](content/configuration.md) | [user_config_service.h](../../../engine/config/user_config_service.h) |
| `ELYSIA_TIME` | 时间相关功能 | [时间与计时器](systems/time-and-timers.md) | [time.h](../../../engine/core/time.h) |
| `ELYSIA_EFFECTS` | 特效服务 | [动画与特效](systems/animation-and-effects.md) | [effect_service.h](../../../engine/effects/effect_service.h) |
| `ELYSIA_LOCALIZATION` | 本地化服务 | [本地化与文本](systems/localization.md) | [localization_service.h](../../../engine/localization/localization_service.h) |
| `ELYSIA_OBJECT_QUERY` | 游戏对象查询 | [游戏对象查询](systems/object-query.md) | [game_object_query_service.h](../../../engine/object_query/game_object_query_service.h) |
| `ELYSIA_RESOURCES` | 查询已加载资源 | [资源加载与使用](content/resources.md) | [resource_service.h](../../../engine/resources/resource_service.h) |
| `ELYSIA_SAVE` | 存档管理 | [存档服务](content/save.md) | [save_service.h](../../../engine/save/save_service.h) |
| `ELYSIA_DEBUG_DRAW` | 调试绘制 | [调试绘制](tools/debug-draw.md) | [debug_draw.h](../../../engine/tools/debug_draw.h) |
| `ELYSIA_CONTROLLERS` | 控制会话、控制器与目标绑定 | [控制器与控制命令](gameplay/control.md) | [controller_service.h](../../../engine/gameplay/control/controller_service.h) |
| `elysia::gameplay::collision::GameplayCollisionService::instance()` | 玩法碰撞绑定、事件监听与攻击实例清理 | [游戏玩法碰撞](gameplay/collision.md) | [gameplay_collision_service.h](../../../engine/gameplay/collision/gameplay_collision_service.h) |

### 日志宏

引擎提供不同级别的日志宏，用于记录运行信息、调试信息、警告与错误。

| 宏名称 | 日志级别 | 用途 |
| --- | --- | --- |
| `ELYSIA_LOG_DEBUG` | Debug | 调试信息 |
| `ELYSIA_LOG_INFO` | Info | 普通运行信息 |
| `ELYSIA_LOG` | Info | Info 级别的便利入口 |
| `ELYSIA_LOG_WARN` | Warn | 警告信息 |
| `ELYSIA_LOG_ERROR` | Error | 错误信息 |
| `ELYSIA_LOG_TERMINATING` | Terminating | 终止级别日志，本身不触发应用退出 |

日志分类、流式消息与错误处理约定见[常用基础工具](tools/utilities.md)。

基本用法：

```cpp
#include "engine/tools/logger.h"

// 在业务函数中记录日志：
ELYSIA_LOG_INFO("Gameplay", "Game initialized.");
ELYSIA_LOG_ERROR("Resources", "Failed to load texture.");
```

## 应用错误展示与退出

Elysia Engine 提供内置应用错误场景，用于处理无法在当前场景中恢复、但仍允许错误界面正常运行的错误。

当游戏无法继续正常运行，且窗口、渲染和必要内置资源仍可用时，可以请求进入错误场景，展示诊断并让用户退出。该场景不提供通用重试或继续游戏功能；可恢复的业务失败由游戏自己的流程处理。

具体使用方式请参阅[应用错误场景](builtin-scenes/application-failure.md)。


## 其他通用工具

除常用服务外，Elysia Engine 还提供了一些可复用的开发工具。

| 工具 | 用途 | 接口定义 |
| --- | --- | --- |
| `RandomGenerator` | 随机数生成 | [random_generator.h](../../../engine/tools/random_generator.h) |
| `Timer` | 计时功能 | [timer.h](../../../engine/tools/timer.h) |
| `Vector2` | 二维向量、方向、距离与归一化 | [vector2.h](../../../engine/core/geometry/vector2.h) |
| `Rect` | 矩形范围、包含与相交等几何判断 | [rect.h](../../../engine/core/geometry/rect.h) |
| `Color` / `colors` | RGBA 颜色值与预定义颜色 | [color.h](../../../engine/core/render/color.h)、[colors.h](../../../engine/core/render/colors.h) |

向量、矩形、颜色以及随机数的用法与边界见[常用基础工具](tools/utilities.md)；`Timer` 的驱动与生命周期见[时间与计时器](systems/time-and-timers.md)。

## 引擎内置场景

Elysia Engine 提供以下内置场景，用于处理常见的应用流程。

| 场景键 | 用途 | 使用文档 |
| --- | --- | --- |
| `StartupLoading` | 项目内容加载、Logo 展示与完成确认 | [启动加载场景](builtin-scenes/startup-loading.md) |
| `Settings` | 游戏设置界面 | [设置场景](builtin-scenes/settings.md) |
| `ApplicationFailure` | 应用错误展示与退出 | [应用错误场景](builtin-scenes/application-failure.md) |

上述键位于 `elysia::builtin::SceneKeys`，定义于 [builtin_scene_keys.h](../../../engine/builtin/builtin_scene_keys.h)。应用会注册这些内置场景，游戏通过路由使用它们。

各场景的配置、数据传递方式及使用约定，请参考对应的使用文档、场景定义和 Payload 头文件。

## 进一步阅读

- [接口与格式参考](../reference/README.md)：查询完整 API 契约和资源格式。

- [架构与模块设计](../architecture/README.md)：了解引擎内部的职责划分与运行机制。
- [开发与贡献指南](../contributing/README.md)：了解代码规范、测试和文档维护要求。
- [设计文档与开发路线](../design/README.md)：查看未来开发计划、设计提案及待完成的架构迁移。
- [返回项目介绍](../../../README.zh-CN.md)：查看项目定位、功能概览与仓库组成。
