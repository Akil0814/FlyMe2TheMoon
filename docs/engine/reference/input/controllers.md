# 控制器、命令与角色

本文说明控制器服务、命令接收者与调度管理器的分工。创建、查询和配置从服务进入，实际意图生产和命令交付由固定步调度完成。

## 游戏 API

[ControllerService](../../../../engine/gameplay/control/controller_service.h) 是游戏侧单例入口。Manager 是唯一所有者；Service 不另存实例或自行调度。

包含 `engine/gameplay/control/controller_service.h` 后，可通过 `ELYSIA_CONTROLLERS->方法(...)` 调用服务。宏展开为 `(::elysia::gameplay::ControllerService::instance())`，不自动开始会话，也不改变接口的返回类型或结果处理要求。

| 接口 | 用途 |
| --- | --- |
| begin_session / end_session / session_active | 显式本地游戏会话 |
| create<T>(create_info, args...) | 创建并登记，返回 expected<`ControllerHandle`, `ControllerError`> |
| get<T>(句柄) / describe(句柄) | 借用查询与绑定描述 |
| remove(句柄) | 返回 expected<void, `ControllerError`>，立即停止后续交付 |
| bind_target / unbind_target | 返回 `ControllerOperation`，显式绑定或解绑 |
| replace_input_map | 返回 `ControllerOperation`，受控替换本地映射 |

所有 Service 调用与控制器回调均在引擎主线程执行；未来网络工作线程应排队提交到主线程，不直接调用这些接口。

`ControllerCreateInfo` 显式给出场景或 Session 作用域；场景作用域还需 `control_context().token()`。创建失败不会隐式开始会话。借用指针不能跨回调、删除或会话边界保留。

## ControlCommand

命令包含控制器句柄、绑定代次、序号、实际执行固定步、持续动作状态、有序事件和本固定步的增量。通用命令不要求 `LocalPlayerId`；玩家身份属于 `LocalPlayerController`。

角色实现 `ControlCommandReceiver::on_control_command(command, fixed_delta)` 和 `on_control_cancelled(reason)`。取消用于清除移动、蓄力等意图，不当作正常松键释放攻击。

引擎没有固定 Move／Jump／Attack 访问器。演示使用游戏层 [CommandView](../../../../game/gameplay/control/command_view.h) 提供便捷读取。内置 `EngineCharacter` 的构造函数显式接收移动动作 ID，不依赖演示动作集合。

物理角色在命令入口写入意图，在物理参与者 fixed_update 中施力；非物理角色可在命令入口按固定 delta 移动。动画等 `Updatable` 保持变步长，不重复移动。

## 自定义来源

`Controller::produce_intent(tick, fixed_delta)` 通过受保护的 `submit(ActionInputResult)` 交付意图。实现示例见[自定义控制器](../../user-guide/gameplay/control.md#自定义控制器)。

## 配置操作结果

`ControllerOperation` 没有 bool 转换，提供 `status()`、`error()` 及 `pending()`／`succeeded()`／`failed()`。三种状态为 Pending、Succeeded、Failed。回调外调用在执行完成后返回终态；回调内允许延迟提交，Pending 只表示请求已受理。

操作结果的保存与轮询示例见[控制器指南](../../user-guide/gameplay/control.md)。

同一控制器的有效换绑／解绑请求覆盖尚未提交的旧目标请求，旧请求以 Superseded 失败；被拒绝的新请求不覆盖旧请求。映射替换按 FIFO 提交。目标和玩家排他性在排队期间同样保留。等待期间控制器被移除时，请求以 InvalidHandle 失败；上下文失效返回 InvalidContext，目标移除返回 InvalidTarget，会话结束返回 NoSession；所有待执行请求均会终结。

提交回调抛异常时，操作以 CallbackFailed 失败并解除受影响绑定；同一控制器的其他待提交操作也失败，其他控制器的请求继续提交。原始异常和后续清理异常随后进入场景失败边界，保留渲染后端失败的优先级。end_session 完成全部失效与清理后才传播通知异常；回调内重开会话返回 SessionEnding。

结果对象只共享小型状态，不持有控制器或场景；丢弃结果不撤销操作，已完成结果可跨会话继续查询。无全局历史表或完成回调。取消期间拒绝 submit；观察映射或生产意图期间若发生取消，该次旧输入不会重新写入命令缓存。
