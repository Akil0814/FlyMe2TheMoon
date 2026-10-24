# 动作映射

动作映射把设备状态与事件转换为游戏定义的动作。本文重点解释持续值和一次性增量的区别，以及映射替换后为什么需要清理旧输入。

`InputActionMap` 只解析 `InputSnapshot`。基于各源帧初状态和有序原始事件重放，再与帧末状态协调。同帧按下再松开、多次点击都保留动作事件顺序；帧状态只表达最后持续值。

游戏定义 `InputActionId`、按键和手柄映射。仓库演示映射位于 [game/input](../../../../game/input/gameplay_input_map.cpp)，不是引擎协议。UI 动作保持独立。

## 数值语义

`InputActionDescriptor` 的 `semantics` 为 `State` 或 `Delta`，默认 `State`。按钮只能是 `State`。持续轴使用 `ButtonInputBinding`、`AxisInputBinding`、`Axis2DInputBinding` 或 `Button2DInputBinding`，继续使用死区、叠加及限幅。

Delta 使用 `PointerDeltaBinding`，选择 MouseX、MouseY、WheelX、WheelY、目标分量和比例。使用逻辑坐标转换后的事件增量，不读取帧汇总再次累计，不套用摇杆死区或 [-1,1] 限幅，不乘帧时间，也不产生 Started／Canceled 事件。

```cpp
#include "engine/input/action/input_action_map.h"

// 调用方将 map 交给本地控制器前，检查本函数是否成功。
bool add_look_delta(elysia::input::InputActionMap& map)
{
    using namespace elysia::input;
    const InputActionId look{"mygame.look_delta"};
    // 返回注册结果；失败时不要假定该动作已可供角色查询。
    return map.register_action(
        {look, InputActionValueType::Axis2D, 0.5f, 0.2f, InputValueSemantics::Delta},
        {{look, PointerDeltaBinding{PointerDeltaAxis::MouseX}},
         {look, PointerDeltaBinding{PointerDeltaAxis::MouseY, InputActionComponent::Y}}});
}
```

同动作不能混合 `State` 与 Delta。鼠标瞄准增量和摇杆转向速度应使用不同动作，由游戏决定如何合成。注册、增加和替换绑定返回验证结果；非法类型或非有限比例被拒绝。

## 控制器中的映射

每个 `LocalPlayerController` 只激活一套映射。游戏可以在登记前构造映射；登记后只允许通过 `ControllerService::replace_input_map(handle, map)` 替换，`input_map()` 只有 const 查询。

替换验证成功后取消旧输入、清空状态／事件／增量并建立释放／回中门控。校验失败保留原映射。替换返回 `ControllerOperation`，回调内 Pending 需要在安全边界后检查最终结果。不提供可变映射引用，也不自动叠加步行、载具和观战映射。

## 命令缓存

持续值保存最新结果，事件保留次数和顺序。零固定步时事件和增量跨帧累积，下一实际固定步交付后清空，后续补跑固定步的增量为零。UI 消费、取消、解绑和映射切换清理相应待执行输入。

每控制器最多 1,024 条待消费事件；溢出取消整份待执行输入并记录诊断，不截取部分攻击。非有限状态、事件或累计增量不能进入交付缓存。

当前不实现复杂组合键、映射栈优先级、配置持久化或相对鼠标模式。

## 精度与参数校验

原始滚轮和 UI 滚轮值使用 float，WheelX／WheelY 增量完整保留小数；滚动容器按小数比例移动，合成手柄滚动仍采用原有整步节奏。Button／Axis1D 绑定仅接受 X 分量，Axis2D 才接受 Y；非法枚举、轴范围和非有限比例被拒绝，失败替换不修改原映射。
