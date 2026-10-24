# 输入、焦点与交互取消

本页集中说明可交互性、焦点登记、设备捕获与取消边界。标准场景已经驱动 UI 输入，不需要游戏再次手动分发。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="navigation"></a>
## 焦点关系与坐标

窗口用 `register_focus_scope` 与 `set_scope_neighbors` 管理跨区域导航；区域维护内部可用目标。控件处理不了的方向动作才继续寻找邻居，嵌套区域保留自身的导航职责。

布局坐标与呈现位移的转换见[元素几何](elements-and-layout.md#ui-element)。滚动区域负责协调内容焦点和滚动位置，接口见[UiScrollContainer](elements-and-layout.md#ui-scroll-container)。

<a id="ui-control"></a>
## UiControl：交互控件基类

`UiControl` 在 `UiElement` 的基础上提供启用与焦点状态，并实现 `UiFocusable` 契约。头文件为 [ui_control.h](../../../../engine/ui/core/ui_control.h)。

`set_enabled` 和 `is_enabled` 控制、查询可交互性；`set_focused` 和 `is_focused` 由焦点系统或容器驱动。派生控件通过 `on_ui_input_event` 处理确认和指针事件。

多控件的焦点互斥由焦点区域、列表、选择组或窗口维护。不要在各控件中分别设置焦点来替代导航系统。详见[输入与焦点](#navigation)。

<a id="ui-focus-scope"></a>
## UiFocusScope：焦点区域契约

`UiFocusScope` 描述一个可以参与窗口导航的区域。它不负责持有其中的控件，普通界面通常通过具体容器使用这套契约。头文件为 [ui_focus_scope.h](../../../../engine/ui/focus/ui_focus_scope.h)。

`focus_scope_element` 提供区域元素，`set_scope_focused` 与 `is_scope_focused` 管理区域焦点。`focused_target` 查询当前目标，`focus_first_available` 选择首个可用目标，`has_focusable_target`、`can_navigate` 和 `contains_focus_point` 用于判断区域能否接受相应操作。

只有实现新的焦点容器时才需要直接实现该接口。窗口级登记见[窗口参考](windows-and-popups.md#ui-window)。

<a id="ui-control-focus-scope-host"></a>
## UiControlFocusScopeHost：持有子项的焦点区域

该基类同时持有子元素并提供区域内导航，适合实现列表、网格等焦点容器。头文件为 [ui_control_focus_scope_host.h](../../../../engine/ui/focus/ui_control_focus_scope_host.h)。

普通场景使用 `focus_first_available` 选择目标，通过 `focused_target`、`has_focusable_target` 和 `can_navigate` 查询状态。需要明确指定目标时使用 `set_focused_target`。

派生容器用 `set_focus_entries` 建立方向邻接关系；`clear_focus_for_gamepad_scroll` 与 `restore_focus_after_gamepad_scroll` 协调手柄滚动时的焦点表现。子项所有权仍遵循[容器规则](elements-and-layout.md#ownership)。

<a id="capture-and-cancel"></a>
## 输入捕获与交互取消

`UiElement::input_capture()` 默认返回无捕获；`UiChildHost` 汇总有效子节点，焦点 `UiTextInput` 报告键盘捕获，`UiWindow` 的模态层和活动弹出层报告捕获。键盘捕获覆盖全部键盘分区；鼠标捕获作用于鼠标所属玩家；手柄捕获只作用于指定 UI 手柄。UI 手柄选择与游戏绑定相互独立。

`cancel_input_interaction()` 清除按钮按住、拖拽等临时交互而保留内容和焦点，用于失焦、设备断开及 UI 手柄切换。它与重置控件状态的 `reset()` 不同。

该虚接口允许交互取消通知抛出异常；仅清状态的控件仍可使用 `noexcept` override。批量取消会继续处理其他仍存活的控件，再传播首次异常。HUD [UiActionButton](controls-and-selection.md#ui-action-button) 会将正在进行的鼠标交互通知为 `Canceled`；其外部状态更新不会产生交互回调。

`UiInteractionMode` 由场景的 `set_ui_interaction_mode` 配置。纯菜单默认 `Navigation`，可用首个手柄按下取得 UI 操作权，不绑定游戏玩家。玩法场景默认 `Pointer`，普通 HUD 只响应指针；打开交互菜单显式启用 `Navigation`。激活文本框始终接管键盘，结束编辑后被抑制按键须释放才能恢复玩法。

<a id="input-runtime"></a>
## 输入与滚动扩展接口

普通界面使用场景和滚动容器；只有编写输入桥接或新容器时才直接使用以下类型。内部调用顺序见[UI 架构](../../architecture/subsystems/ui/README.md#frame-flow)。

`UiAction` 表达导航、`Confirm`、`Cancel`、`Tab` 和编辑／翻页动作；`UiInputEvent` 的 `type` 决定哪些载荷字段有效。接口定义见 [ui_input_types.h](../../../../engine/ui/input/ui_input_types.h)，不要把未参与当前事件类型的默认载荷当作有效输入。

| 类型 | 调用方法 | 作用 |
| --- | --- | --- |
| `UiInputRouter` | `route_frame`、`route_event`、`synthesize_events`、`reset_transient_state` | 将原始输入帧与事件转为 UI 输入帧与事件，并生成手柄滚动等合成事件。 |
| `UiInputState` | `set`、`is_pressed`、`is_just_pressed`、`is_just_released` | 保存并查询 UI 动作的按住状态和本帧按下、释放边沿。 |
| `UiGamepadScrollSynthesizer` | `synthesize(raw_frame)`、`reset` | 把连续手柄输入转换为可选滚动事件。 |
| `UiScrollState` | `set_axis`、viewport/content size、偏移、模拟步、比例、`scroll_by` | 计算独立滚动状态并限制偏移范围。 |
