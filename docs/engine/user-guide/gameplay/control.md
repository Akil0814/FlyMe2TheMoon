# 控制器与控制命令

本文按会话、创建、绑定和命令消费的顺序介绍角色控制。读完后应能区分控制器句柄、绑定操作结果和目标对象的生命周期。

控制器把本地输入或自定义意图转换为固定步消费的 `ControlCommand`。游戏层通过 `ELYSIA_CONTROLLERS` 访问控制器服务，管理会话、创建控制器并绑定目标。这里的会话是游戏会话，不代表网络连接。

包含 `engine/gameplay/control/controller_service.h` 后即可使用 `ELYSIA_CONTROLLERS->方法(...)`。这个便利宏等价于 `::elysia::gameplay::ControllerService::instance()`，只简化服务访问；会话仍需显式开始，创建错误和绑定操作结果仍需处理。`ControllerManager` 负责内部调度，游戏侧统一通过服务操作控制器。

先用[输入指南](../systems/input.md)定义动作和设备映射，再按本页接入目标对象。仓库的游戏侧封装位于 `game/gameplay/control/` 和 `game/input/`，使用 `example` 命名空间；这些辅助函数与动作 ID 不是引擎内置游戏协议。

## 接入顺序

1. 由游戏流程的唯一负责人调用 `begin_session()`，处理返回的 `std::expected`；不要每进入一个子场景都重复开始会话。
2. 进入 `GameplayScene`，取得有效的 `control_context()`。
3. 将目标对象加入该场景。目标必须同时实现 `ControlCommandReceiver`。
4. 创建控制器并绑定目标；检查绑定操作的最终结果。
5. 退出游戏会话时调用 `end_session()`。结束会话会释放控制器，旧句柄不应继续使用。

## 创建本地玩家控制器

以下辅助函数可放在游戏代码中，在活动 `GameplayScene` 的输入或更新逻辑中调用。调用方已开始会话，`player` 已登记在场景玩家注册表，`map` 已按设备配置，`target` 已加入该场景且实现命令接收接口。

```cpp
#include "engine/gameplay/control/controller_service.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/tools/logger.h"

#include <expected>
#include <optional>
#include <utility>

std::expected<elysia::gameplay::ControllerHandle,
              elysia::gameplay::ControllerError>
create_player_control(elysia::gameplay::GameplayScene& scene,
                      elysia::input::LocalPlayerId player,
                      elysia::input::InputActionMap map)
{
    using namespace elysia::gameplay;
    // 作用域关联场景上下文，映射的所有权随创建参数移交给控制器。
    return ELYSIA_CONTROLLERS->create<LocalPlayerController>(
        ControllerCreateInfo{ControllerScope::Scene,
                             scene.control_context().token()},
        player, std::move(map));
}
```

创建结果失败时不取值；成功后保存句柄，并调用 `bind_target(handle, scene.control_context(), target)`。设备与动作映射的具体构造见[动作映射](../systems/input.md)。句柄不是裸指针，也不延长场景或目标生命。

## 绑定结果必须确认

`bind_target`、`unbind_target`、`replace_input_map` 返回 `ControllerOperation`。调用可能在内部回调边界延迟提交，不能把“函数返回”理解为“绑定完成”。

```cpp
// 放在返回 void 的接入函数中；scene、target 均满足前述前提。
elysia::gameplay::ControllerService* service = ELYSIA_CONTROLLERS;
const std::expected<elysia::gameplay::ControllerHandle,
    elysia::gameplay::ControllerError> created =
    create_player_control(scene, player, std::move(map));
if (!created)
{
    ELYSIA_LOG_ERROR("control", "Controller creation failed: "
        << static_cast<int>(created.error()));
    return;
}
const elysia::gameplay::ControllerHandle handle = *created;
const elysia::gameplay::ControllerOperation operation =
    service->bind_target(handle, scene.control_context(), target);
if (operation.failed())
{
    const std::optional<elysia::gameplay::ControllerError> error = operation.error();
    if (error)
        ELYSIA_LOG_ERROR("control", "Target binding failed: " << static_cast<int>(*error));

    // 本次新建控制器不再使用；清理失败单独记录，保留原始绑定失败。
    const std::expected<void, elysia::gameplay::ControllerError> removed =
        service->remove(handle);
    if (!removed)
        ELYSIA_LOG_ERROR("control", "Controller removal failed: "
            << static_cast<int>(removed.error()));
    return;
}
// 保存 handle；若 operation.pending()，同时保存 operation，后续检查最终状态。
// 只有 operation.succeeded() 后，才宣布目标已经绑定成功。
```

`Pending` 不是失败，也不要在主线程忙等它。后续状态变为 `Failed` 时处理 `error()`，例如上下文失效、目标忙、玩家忙、无效映射或请求被后续操作取代。操作对象只保留结果，不保留控制器、场景或目标。

## 命令消费与取消

以下目标类与[输入指南](../systems/input.md)中的 `game.move` 映射配套。先加入 `GameplayScene`，再使用 bind_target 绑定。它没有物理刚体，命令直接推进世界位置；绘制可按[游戏对象](../scene/game-objects.md)添加。

```cpp
#include "engine/core/game_object.h"
#include "engine/gameplay/control/control_command.h"

class ControlledObject final : public elysia::core::GameObject,
                               public elysia::gameplay::ControlCommandReceiver
{
public:
    ControlledObject() : GameObject(elysia::core::DepthLayer::Character)
    {
    }

    void on_control_command(const elysia::gameplay::ControlCommand& command,
                            double fixed_delta) override
    {
        // 命令按固定步交付；本对象没有刚体，直接移动且不再叠加全局倍率。
        _movement = command.state.axis2d(elysia::input::InputActionId{"game.move"});
        set_position(position() + _movement * (100.0f * static_cast<float>(fixed_delta)));
    }

    void on_control_cancelled(elysia::gameplay::InputCancelReason) override
    {
        // 失焦、暂停或解绑后清除持续意图，不沿用上一条命令。
        _movement = {};
    }

private:
    elysia::core::Vector2 _movement{};
};
```

目标实现 `on_control_command(const ControlCommand&, double fixed_delta)` 和 `on_control_cancelled(InputCancelReason)`。命令包含持续状态、一次性事件、累计增量以及 tick/sequence。控制器在实际固定步交付命令；事件和增量交付后清空，不能假设每个渲染帧都收到一条命令。

取消回调应清除移动、蓄力等持续意图，避免暂停、设备变化或解绑后沿用旧输入。物理目标可在命令回调中记录意图，再在自己的物理阶段消费；不要同时在逐帧更新中重复推进同一运动。

取消回调中的异常会在必要清理完成后传播到场景异常边界。控制器取消失败不阻止目标收到通知，也不阻止其他控制器清理。延迟配置提交异常使操作终结为 `CallbackFailed` 并解除受影响绑定；游戏应处理场景故障，不继续使用旧绑定。结束会话时全部句柄先失效，取消回调内重新开始会话返回 `SessionEnding`。

自定义 AI 等来源继承 `Controller`，在 `produce_intent(tick, fixed_delta)` 中通过受保护的 `submit(ActionInputResult)` 提交结果。无需伪造设备事件；这也不意味着引擎已提供网络同步。

## 自定义控制器

下面是完整类型定义，演示不依赖设备的恒定轴输入。通过 `ELYSIA_CONTROLLERS->create<ConstantController>(...)` 创建，再绑定目标；创建时传入目标认识的 Axis1D 动作 ID。本例没有 AI 决策或网络同步。

```cpp
#include "engine/gameplay/control/controller.h"
#include <utility>

class ConstantController final : public elysia::gameplay::Controller
{
public:
    explicit ConstantController(elysia::input::InputActionId axis)
        : _axis(std::move(axis))
    {
    }

protected:
    void produce_intent(std::uint64_t, double) override
    {
        elysia::input::ActionInputResult intention;
        intention.frame.set(_axis, elysia::input::InputActionValueType::Axis1D,
                            {0.5f, 0.0f});
        // 提交给统一调度；不要从控制器直接调用目标对象。
        submit(std::move(intention));
    }

private:
    elysia::input::InputActionId _axis;
};
```

本地控制器可覆盖 `on_mapped_input` 观察唯一活动映射的结果，不再解析另一套设备映射。需要固定步执行的辅助行为应留给 `produce_intent`。目标被移除时，工具还需清理自身保存的业务引用。

## 作用域与清理

会话创建失败时停止创建控制器，例如重复开始会话应由游戏流程解决。`unbind_target(handle)` 只解除目标关联，`remove(handle)` 释放控制器；两者不要混用。get 返回的控制器指针只是临时借用，remove、上下文销毁或 end_session 后不可继续保存使用。

场景作用域绑定创建时的上下文标记；上下文重置、场景销毁或会话结束会使相关控制器失效。Session 作用域可跨场景保留控制器，但离开旧场景仍会解绑目标，需要在新场景显式重新绑定。

仅离开缓存场景不等于重置上下文。不要在每次 `on_enter()` 中重复创建未清理的控制器。对象移除、场景离开和暂停有引擎侧取消流程；游戏侧仍需停止使用失效句柄及 `get()` 返回的借用指针。`remove()` 返回错误时应区分已经失效与其他接入问题。

完整公开契约见[接口参考](../../reference/input/controllers.md)。

## 参考

- [公开服务](../../../../engine/gameplay/control/controller_service.h)、[操作结果与错误](../../../../engine/gameplay/control/controller_types.h)
- [控制器与目标契约](../../../../engine/gameplay/control/control_command.h)
- [GameplayScene](scene.md)、[返回使用指南](../README.md)
