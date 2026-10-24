# 窗口、弹窗与提示

本页把窗口级表面的所有权、注册条件、交互优先级和解除关系放在一起。提高绘制顺序不会自动获得模态或弹出层行为。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="surface-contract"></a>
## 表面与注册的共同规则

| 表面 | 所有权与登记 | 交互 |
| --- | --- | --- |
| 覆盖层、对话框 | 窗口直接持有元素，注册只增加行为关系 | 可模态，关闭时尝试恢复之前的焦点区域 |
| 临时弹出层 | 实现者持有内容，窗口借用弹出层协议 | 协调活动弹出层的输入与绘制优先级 |
| 提示 | 提示组件持有内容并借用触发元素 | 被动显示，不能代替交互式对话框 |

`UiTransientPopup` 通过 `popup_owner`、`is_open`、`contains_popup_point`、`on_popup_input_event` 和绘制接口提供协议，标准实现为 `UiDropdown`。活动弹出层可以阻挡背景提示。

覆盖层、`UiDialog` 和 `UiConfirmationDialog` 必须先成为 `UiWindow` 的直接子元素，再调用注册接口；它们只在嵌套容器中时会被拒绝。下拉控件则允许位于窗口子树的嵌套容器中。注册不转移所有权。销毁窗口、显式注销、或元素从子元素树移除都会使组件的借用窗口指针失效；实现会接收 `on_window_detached`。

```cpp
auto dialog = std::make_unique<elysia::ui::UiDialog>(dialog_rect);
elysia::ui::UiDialog* dialog_ptr = dialog.get();
window.add_child(std::move(dialog));
// 只有成功注册为覆盖层后，才能按弹窗方式打开。
if (dialog_ptr->register_with_window(window))
{
    dialog_ptr->open();
}
else
{
    ELYSIA_LOG_ERROR("ui", "Dialog must be a direct child of its window.");
}
```

覆盖层使用 `open_overlay(element)`/`close_overlay(element)`；对话框、下拉控件、提示使用自己的 `open`/`close` 或注册 API。不要混用两套调用入口。

上述片段需包含对话框和日志头文件；`window` 与 `dialog_rect` 已由调用方准备。

<a id="ui-window"></a>
## UiWindow：窗口与表面协调

头文件：[ui_window.h](../../../../engine/ui/window/ui_window.h)。作为根容器持有 UI 子树，并协调窗口级焦点、覆盖层、弹出列表与提示。

主要 API：

- 样式：`set_base_style`、`set_style_overrides`、`set_hover_focus_enabled`、`set_on_cancel`。

Cancel 先交给当前焦点控件退出局部调整，未消费时再执行窗口取消回调；覆盖层也先允许其焦点控件处理，再按 `close_on_cancel` 关闭，事件不继续传给背后窗口。瞬时弹出层仍优先处理自己的取消。
- 焦点：`register_focus_scope`、`unregister_focus_scope`、`set_scope_neighbors`、`set_focused_scope`、`focus_first_available_scope`。
- 覆盖层：`register_overlay`、`unregister_overlay`、`open_overlay`、`close_overlay`、`is_overlay_open`、`overlay_options`。
- 弹出层与提示：`register_transient_popup`、`unregister_transient_popup`、`activate_transient_popup`、`register_tooltip`、`unregister_tooltip`。

示例见[窗口表面专题](#surface-contract)。对话框和覆盖层必须是窗口的直接子元素；焦点区域和下拉控件的弹出层可来自窗口内的嵌套子树，不能将两种登记条件混为一谈。

<a id="ui-window-调用流程"></a>
### 调用流程

1. 由场景创建并持有 `UiWindow`。
2. 使用 `add_child`/`create_child` 构建普通 UI 树。
3. 对独立导航区域调用 `register_focus_scope`；通过 `UiFocusScopeNeighbors` 描述跨区方向。
4. 对对话框或覆盖层完成所有权转移后注册窗口表面。
5. 场景每帧将输入、更新和绘制交给窗口。

焦点注册跟随窗口的子树所有权：临时隐藏、停用窗口、焦点区域或其祖先，不会移除注册、注册顺序或跨区域邻接关系。不可用期间，焦点区域不参与焦点选择和导航；恢复可用后，无需重新注册即可通过现有焦点恢复流程继续导航，并保留仍有效的历史焦点。

窗口清理焦点注册时，仅移除已销毁或已脱离窗口子树的焦点区域，并清理对应的当前／历史焦点缓存。需要永久取消注册时，使用 `unregister_focus_scope`。

`register_overlay` 对已销毁或非直接子元素返回 `false`；重复登记同一元素会替换原选项。`overlay_options(element)` 对未登记元素返回空，否则返回内部选项的借用指针。注册表增删也可能使指针失效，应在使用前重新查询。修改放置参数后需使窗口布局失效；开启、关闭请使用对应方法，以同步焦点和可见性，不能只改 `open` 字段。

### UiOverlayOptions：放置与关闭规则

以下是结构体自身的默认值；对话框包装器有后文说明的默认策略。定义见 [ui_overlay.h](../../../../engine/ui/window/ui_overlay.h)。

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `open` | `true` | 登记后的初始打开状态，与元素寿命分开 |
| `modal` | `true` | 阻止底层普通内容交互 |
| `close_on_cancel` | `true` | 取消动作关闭覆盖层 |
| `close_on_outside_click` | `true` | 外部点击关闭覆盖层 |
| `placement` | `UiOverlayPlacement::Center` | 也支持 `LeftDrawer`、`RightDrawer`、`TopSheet`、`BottomSheet` |
| `transition` | `None` | 可选 `Slide` 呈现过渡 |
| `fallback_size` | `{360, 220}` | 元素没有可用显式尺寸时的回退尺寸 |
| `order` | `1000` | 登记时写入元素的绘制顺序 |

<a id="dialogs"></a>
## 覆盖层对话框：UiDialog 与 UiConfirmationDialog

对话框用于临时显示说明或请求确认。普通窗口负责持有和调度对话框，对话框自身维护内容与打开状态。

相关头文件：[ui_dialog.h](../../../../engine/ui/composites/ui_dialog.h)、[ui_confirmation_dialog.h](../../../../engine/ui/composites/ui_confirmation_dialog.h)。

共同调用顺序：将对话框作为窗口的直接子元素加入 → 检查 `register_with_window(window, options)` 返回 bool → `open()`/`close()`。位于嵌套容器中的对话框即使最终由同一窗口管理，也会注册失败。未注册、已注销或窗口销毁后的 open/close 安全无操作。

- `UiDialog`：`set_title_content`、`set_body_content`、`set_action_content`；`set_body_scroll_enabled` 和 `set_header_visible` 控制阅读布局。
- `UiConfirmationDialog`：用 `UiConfirmationDialogConfig` + `set_config` 配置标题、消息、确认、取消和关闭动作；`set_on_confirm` 注册确认动作。确认时先关闭再执行回调。

传入完全等于默认构造值的选项时，`UiDialog` 使用当前样式中的 `overlay_defaults`；`UiConfirmationDialog` 改为初始关闭、模态、允许取消关闭、禁止外部点击关闭、居中且无过渡。需要不同策略时显式配置选项；对话框的当前尺寸也会参与回退尺寸设置。

<a id="ui-dropdown"></a>
## UiDropdown：下拉选择

头文件：[ui_dropdown.h](../../../../engine/ui/composites/ui_dropdown.h)。通过临时弹出列表提供选项选择。

<a id="ui-dropdown-配置与打开"></a>
### 配置与打开

`set_options`/`add_option` 后 `set_selected_index`；将其纳入窗口子树后调用 `register_with_window(window)`；`open`、`close`、`toggle`、`is_open` 控制弹出列表。`set_on_selection_changed` 接收选中索引。注销使用 `unregister_from_window`。

禁用选项仍然显示但不会成为键盘/手柄焦点。没有有效选项或未注册窗口时 `open()` 不会打开。`set_options` 会重建选项行，并尝试保留原索引；原索引越界或对应新选项禁用时，改选首个可用项，没有可用项时清空选择并关闭列表。保留的是索引，不是选项内容的业务身份。`set_options` 本身不发送选择变化回调；`set_selected_index` 在合法索引实际变化时会触发回调，并非所有程序设置都是静默操作。

<a id="ui-tooltip"></a>
## UiTooltip：被动提示

`UiTooltip` 在窗口层显示触发元素的补充说明，不承担交互式对话框的职责。头文件为 [ui_tooltip.h](../../../../engine/ui/composites/ui_tooltip.h)。

`bind_trigger` 借用触发元素，`set_content` 接管提示内容，`release_content` 归还内容所有权。使用 `set_show_delay` 设置延迟，再用 `register_with_window` 开始观察，或用 `unregister_from_window` 停止观察。

`open`、`close` 和 `is_open` 提供显式显示控制。触发元素的借用关系不延长其生命。正常使用时，提示组件与触发元素都应位于所登记窗口的存活子树中；窗口维护阶段会清除已离开子树的触发元素或提示登记。显示内容仍由提示组件独占。详见[窗口表面规则](#surface-contract)。
