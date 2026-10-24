# 按钮、选择与调值

本页按“触发操作、保持选择、调整数值、切换页面”组织控件。控件只维护界面状态，业务效果由宿主回调实现。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="callbacks"></a>
## 共同回调约定

`set_on_<event>` 替换此前回调，不累加。回调可修改同一 UI 树的状态，但涉及销毁当前对象、切换场景或替换父内容时应在当前事件结束后执行或使用更高层调度。

- `set_on_click`: 无参数。
- `set_on_selection_changed`: `UiRadioGroup` 或页签回调是 `std::optional<std::size_t>`；下拉控件和 `UiOptionSelector` 是 `std::size_t`；单个单选按钮回调无参数。`UiButtonGroup` 没有这个回调接口。
- `set_on_toggled`: `UiCheckboxState`。
- `set_on_value_changed`: `float`。
- 文本输入控件的 `std::string_view` 只在回调调用期间可用，需要保存时复制为 `std::string`。

程序赋值是否通知回调取决于具体接口：Checkbox 的 `set_state` 静默更新，Dropdown 的有效索引变化会通知。不要用一条通用规则代替下文的具体契约。

<a id="ui-button"></a>
## UiButton：按钮

头文件：[ui_button.h](../../../../engine/ui/widgets/ui_button.h)。用于普通确认和选择操作，可显示文字、图标或状态纹理。

<a id="ui-button-内容与回调"></a>
### 内容与回调

使用 `UiButtonConfig` 构造或 `set_button_config` 原子更新；文本可用 `set_text_content` 直接更新，图标或完整纹理集通过 `UiButtonConfig::content` 中的 `UiButtonIconContent`/`UiButtonTextureSetContent` 提供。`set_on_click` 替换点击回调，`prepend_on_click` 在现有回调前追加一个动作。

`set_state_textures`、`set_sounds`、`set_padding` 配置表现。样式与视觉角色遵循[样式专题](style-and-theme.md#style-contract)。

`UiButtonContent` 与纹理指针均不转移外部纹理所有权。

<a id="ui-button-交互与状态"></a>
### 交互与状态

按钮只有在可见、活跃且启用时，才响应确认动作和主鼠标按键。`set_enabled(false)` 会切换到禁用样式并阻止点击回调。`set_focused` 通常由焦点区域设置；手工设置适合复合组件同步内部视觉状态，不应代替导航系统。

以下片段位于拥有 `button` 的宿主成员函数中，`save_settings()` 由游戏实现。

```cpp
button.set_text_content(elysia::ui::ui_text_key("common.confirm"));
// 宿主负责验证、应用和保存设置；回调不能比宿主活得更久。
button.set_on_click([this]
{
    save_settings();
});
button.set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
```

<a id="ui-action-button"></a>
## UiActionButton：HUD 动作槽位

头文件：[ui_action_button.h](../../../../engine/ui/widgets/ui_action_button.h)。用于 HUD 的通用动作槽位，可展示技能、物品格或暂停按钮。它继承 `UiElement`，接收鼠标事件，不参与焦点导航，不处理键盘／手柄 `Confirm`，也不绑定游戏动作、玩家或控制器。

<a id="ui-action-button-内容与状态"></a>
### 内容与状态

使用矩形、位置＋尺寸或 `from_center` 构造。`set_content` 接受空内容、`UiTextContent` 或 `UiActionButtonIconContent`；图标纹理由调用方持有，可指定 `source_rect` 显示图集区域。图标保持源区域宽高比并适配内部空间，文字使用现有本地化与字体服务。

- `set_key_hint` 设置左下角按键提示，`set_badge_text` 设置右上角角标；两者接受原始文字或本地化键。数量格式化与按键名称解析由游戏层完成。
- `set_selected` 控制独立选中边框。
- `set_overlay_ratio` 控制从底部向上的矩形遮罩：0 无覆盖、1 完全覆盖；有限值限制到 `[0,1]`，非有限值按 0 处理。它不计时，也不影响是否可操作。
- `set_external_pressed` 只同步视觉状态，不执行回调。`is_pointer_pressed` 查询鼠标按住状态，`is_external_pressed` 查询外部状态，`is_pressed` 是两者的并集。普通鼠标释放不修改外部状态。
- `set_enabled(false)` 禁止新鼠标交互，并取消当前交互。系统或显式取消清除鼠标按住、hover 和外部按下表现；游戏层在下一次状态同步时可以重新提供外部状态。

绘制顺序为背景、内容、遮罩、普通边框、选中边框、提示、角标。遮罩与选择独立于启用状态。样式通过 `set_base_style`、`set_style_overrides` 和 `clear_style_overrides` 配置；主题管理器从按钮主题派生基础颜色，保留局部覆盖。`set_font_source_override` 可指定所有文字的字体来源，主文字使用 Button 排版角色，提示与角标使用 Caption。

控件采用布局坐标输出命令，由场景／父容器应用呈现位移、透明度与裁剪；命中测试使用包含祖先位移的呈现坐标。

<a id="ui-action-button-交互与生命周期"></a>
### 交互与生命周期

`set_on_interaction` 接收 `UiActionButtonInteraction`：

| 阶段 | 含义 |
| --- | --- |
| `Pressed` | 主鼠标键在可交互的控件内部按下；重复按下不重复通知。 |
| `Released` | 由本控件开始的鼠标交互结束；`clicked` 仅在释放时仍可交互且指针位于内部时为真。 |
| `Canceled` | 正在进行的鼠标交互因禁用、显式取消或系统输入取消而终止；不算成功点击。 |

移出控件不会结束按住，再移回后仍可成功点击；在外部释放也会通知 `Released`，但 `clicked=false`。鼠标移动和其他按键不被消费。控件不捕获整个设备类别，只消费自己开始的主鼠标按下／释放。

状态在回调前更新。回调可以移除当前控件、修改 UI 树或抛出异常；异常沿现有输入边界传播。批量系统取消使用稳定快照，继续清理其他仍存活的控件，再传播首次异常。取消回调中新增的控件不属于当前批次。

`reset()` 和析构不会执行回调。游戏层在隐藏、停用、移除 HUD 前，应显式调用 `cancel_input_interaction()`，并结束自身动作。游戏层的输入取消与场景生命周期也应清理游戏动作，不应依赖控件析构通知。控件允许暂停时接收 UI 输入；暂停期间的具体可操作条件由游戏层通过启用状态与回调逻辑控制。

<a id="ui-action-button-游戏层连接示例"></a>
### 游戏层连接示例

可运行示例位于 UI 组件展示场景的「控件 / Controls」页，源码见 [ui_component_gallery_hud.cpp](../../../../game/showcase/ui/ui_component_gallery_hud.cpp)。E 或鼠标按下触发技能，3 秒后冷却恢复；1／2 或鼠标点击使用物品；P 或点击暂停按钮只暂停示例的冷却与物品操作。切换页签会清理交互状态，隐藏页不处理这些快捷键。

以下回调中的游戏方法由游戏层自行实现。它们可以提交输入请求，也可以执行暂停等场景操作；控件自身不选择这些路径。

```cpp
using namespace elysia::ui;

// 按住型技能：点击 HUD 产生游戏输入请求，外部 gameplay 状态只更新视觉。
skill_button.set_key_hint(ui_raw_text("E"));
skill_button.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Pressed)
    {
        begin_skill_from_hud();
    }
    else // Released 或 Canceled：均结束 HUD 产生的按住请求。
    {
        end_skill_from_hud();
    }
});
// game 的 HUD 更新：不要在这里重复执行技能。
skill_button.set_external_pressed(skill_action_is_held());
skill_button.set_overlay_ratio(skill_cooldown_remaining_fraction());
skill_button.set_enabled(can_start_skill());

// 物品格：一次成功点击改变游戏选中项；数量与选择由游戏状态反馈。
item_slot.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
    {
        select_item_slot();
    }
});
item_slot.set_badge_text(ui_raw_text(std::to_string(item_count())));
item_slot.set_selected(item_slot_is_selected());

// 暂停按钮：game 的键盘快捷键与 HUD 点击都调用同一个 game 方法。
pause_button.set_on_interaction([this](const UiActionButtonInteraction& event)
{
    if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
    {
        toggle_pause();
    }
});
```

按下就开始冷却的技能，应将“可开始新技能”与“当前按住是否继续”在游戏层区分：禁用控件会主动取消当前鼠标交互。多个输入来源共同控制按住动作时，游戏层应合并来源状态，避免一个来源释放终止另一个来源的按住。

<a id="selection-controls"></a>
## 基础选择控件：UiCheckbox 与 UiRadioButton

复选框维护独立的三态值，单选按钮表达一项是否被选中。多个单选项之间的互斥由选择组维护，不由单个按钮自动完成。

相关头文件：[ui_checkbox.h](../../../../engine/ui/widgets/ui_checkbox.h)、[ui_radio_button.h](../../../../engine/ui/widgets/ui_radio_button.h)。

<a id="selection-controls-uicheckbox"></a>
### UiCheckbox

`UiCheckboxState` 有 `Unchecked`、`Checked`、`Indeterminate` 三态。`set_state` 直接写完整状态；`set_checked`、`is_checked`、`is_indeterminate` 是布尔快捷 API；`toggle` 推进状态。`set_on_toggled` 接收最终 `UiCheckboxState`。用户交互和显式 `toggle()` 都会在状态改变时触发回调；`set_state()` 与 `set_checked()` 只同步状态，不触发回调。`toggle()` 在 Checked 与 Unchecked 之间切换，Indeterminate 切换为 Checked，不是三态轮流推进。带标签复选框沿用同一规则。

`set_mark_style` 选择 Checkmark、FilledBox 或 RadioDot；`set_state_textures`/`clear_state_textures` 管理可选纹理皮肤；`set_sounds` 管理交互声音。`UiCheckboxConfig` 可一次设置纹理、声音、局部样式覆盖与 mark 样式。

<a id="selection-controls-uiradiobutton"></a>
### UiRadioButton

`set_selected`/`is_selected` 写入或读取选择状态；`select()` 请求选择并在状态变化时触发 `set_on_selection_changed`。它实现 `UiRadioItem`，但自身不清除同级单选按钮；需要互斥时放进 `UiRadioGroup`。

`UiRadioButtonConfig`、`set_radio_button_config`、sounds、内边距、base style/overrides 的使用方式与 Checkbox 对应。单选按钮的回调不提供索引；需要知道选中了哪一项时，使用选择组的索引回调。

<a id="selection-groups"></a>
## 选择组：UiButtonGroup 与 UiRadioGroup

选择组将一组按钮或单选项组织为至多一个选中项，适合菜单和互斥设置。组负责协调选择，调用方读取选择结果。

相关头文件：[ui_button_group.h](../../../../engine/ui/containers/ui_button_group.h)、[ui_radio_group.h](../../../../engine/ui/containers/ui_radio_group.h)。

两者都继承 `UiListContainer`，并维护至多一个选择项：

- `selected_index()` 返回 `std::optional<std::size_t>`；
- `set_selected_index(index)` 成功时返回 `true`；无效索引返回 `false`；
- `UiRadioGroup::set_on_selection_changed` 接收新的可选索引，实际选择变化时通知；`UiButtonGroup` 通过 `selected_index()` 查询结果，没有组级选择变化回调。

`UiButtonGroup` 使用 `add_button(std::unique_ptr<UiButton>)`，默认开启 `auto_select_first`，可用 `set_auto_select_first` 修改。组选择在按钮原有点击回调之前更新，因此可在按钮回调中查询 `selected_index()`。程序调用 `set_selected_index` 只更新选择与视觉角色，不触发按钮点击。源码见 [ui_button_group.h](../../../../engine/ui/containers/ui_button_group.h)。

按钮的业务点击回调应在 `add_button` 之前设置；加入后再调用 `set_on_click` 会替换包含组选择逻辑的回调。需要在组逻辑之后处理业务时，保留加入前设置的回调。

`UiRadioGroup` 接受作为子元素的 `UiRadioItem`（`UiRadioButton` 或 `UiLabeledRadioButton`）。`set_selected_index` 同步成员的选中状态，选择实际变化时调用组回调。不要同时手动修改子项选择和组选择来维持互斥。

<a id="drag-value-controls"></a>
## 拖拽与数值调节：UiDragHandle 与 UiSlider

拖拽柄处理指针移动，滑条则将交互转换为范围内的数值。两者的回调反映交互结果，业务如何使用数值由调用方决定。

相关头文件：[ui_drag_handle.h](../../../../engine/ui/widgets/ui_drag_handle.h)、[ui_slider.h](../../../../engine/ui/widgets/ui_slider.h)。

<a id="drag-value-controls-uidraghandle"></a>
### UiDragHandle

`UiDragHandleConfig` 可一次设置拖拽轴、可选边界和局部样式覆盖。`set_drag_axis` 选择 Free/Horizontal/Vertical；`set_drag_bounds` 会将拖拽结果限制在边界内；`clear_drag_bounds` 允许无边界拖拽。`set_on_dragged` 与 `set_on_drag_ended` 接收当前中心。`begin_drag_from_pointer`/`cancel_drag` 适合程序化控制。

<a id="drag-value-controls-uislider"></a>
### UiSlider

滑条内部组合数值条、拖拽柄和可选的数字显示。先 `set_range`，再 `set_value`；`set_step` 为 `nullopt` 时连续变化，否则吸附到正步长。`set_orientation` 选择横/竖，`set_value_display` 选择 None/Value/Percent，`set_on_value_changed` 只在最终值实际改变时触发。

`UiSliderConfig` 可原子设置范围、初值、步长、方向、显示方式与样式。数值格式 API 与 [数值展示](text-and-values.md#value-display) 的 `UiNumber` 规则对应。

<a id="labeled-controls"></a>
## 带标签选择控件

这两种控件将选择标记与文字标签组合起来，适合设置项。标签属于同一交互控件，读者点击文字时也能触发对应选择。

相关头文件：[ui_labeled_checkbox.h](../../../../engine/ui/composites/ui_labeled_checkbox.h)、[ui_labeled_radio_button.h](../../../../engine/ui/composites/ui_labeled_radio_button.h)。

两者把选择标记与标签组合为单一交互控件。构造时传对应 Config，或使用 `set_labeled_*_config` 更新文本、位置关系、间距和基础样式。共同布局 API：`set_label_placement`、`set_text_placement`、`set_label_spacing`、`set_typography_role`。

- LabeledCheckbox：`set_state`、`set_checked`、`toggle`、`set_on_toggled`；文本点击也会路由到选择标记。
- LabeledRadioButton：`set_selected`、`is_selected`、`set_on_selection_changed`；放入 `UiRadioGroup` 获得互斥。

<a id="ui-option-selector"></a>
## UiOptionSelector：横纵选项切换

头文件：[ui_option_selector.h](../../../../engine/ui/composites/ui_option_selector.h)。用于名称居中、两端箭头切换的离散选项，例如难度或角色名称。整个组件只占一个焦点，内部按钮和标签由组件持有。

`set_options` 接受 `UiOptionSelectorOption` 列表，每项包含 `UiTextContent content` 与 `bool enabled`。`selected_index()` 返回可选索引；`set_selected_index` 拒绝越界及禁用项，有效赋值返回 `true`，仅实际变化通知 `set_on_selection_changed`，包括程序赋值。`select_previous`、`select_next` 跳过禁用项，仅切换成功时返回 `true`。

列表更新静默保留仍有效且启用的当前索引，否则选中第一个启用项；空列表或全禁用时显示 `--`。`clear_options()` 清空列表。`set_wrap_selection(true)` 开启首尾循环，默认关闭；只有一个启用项时两侧箭头均禁用，不会循环到自身。选项的业务数据由宿主按索引映射。

`set_orientation` 使用 `UiOptionSelectorOrientation::Horizontal`（默认）或 `Vertical`。横向左／右切换，纵向上／下切换，左／上表示前一项。未调整时方向键用于父级导航；确认或点击中央名称进入调整，调整时消费四个方向键，非当前轴和端点按键不会转移焦点。`is_adjusting()` 查询当前状态。

再次确认、取消或点击中央名称退出调整，保留已即时生效的选择。鼠标箭头在同侧按下并释放后直接切换，不要求先进入调整，也不改变当前调整状态。失焦、禁用、Tab、取消交互、改变方向和替换列表会结束调整；`reset()` 还会清空列表、回调及样式覆盖，并恢复默认横向、不循环状态。

`UiOptionSelectorStyle` 与 Overrides 遵循共同样式契约：`chrome` 控制整体背景和焦点／调整态边框，`arrow` 控制按钮及箭头颜色，`text` 控制名称颜色。`arrow_extent`、`spacing`、`padding` 默认分别为 `40`、`8`、`4`，沿当前主轴布局；窄区域压缩两端与间距，中央文字居中且只缩小适配。箭头由几何线条绘制，不依赖字体特殊字符。现有主题管理器会同步整体样式，局部覆盖继续保留。

<a id="tabs"></a>
## 页签与页面：UiTabBar、UiTabView 和 UiTabContainer

页签界面需要同时管理选择按钮与页面内容。普通界面使用 `UiTabContainer` 即可；只有需要自定义组合时，才分别使用 `UiTabBar` 和 `UiTabView`。相关头文件为 [ui_tab_bar.h](../../../../engine/ui/composites/ui_tab_bar.h)、[ui_tab_view.h](../../../../engine/ui/containers/ui_tab_view.h) 和 [ui_tab_container.h](../../../../engine/ui/composites/ui_tab_container.h)。

<a id="tabs-组合入口"></a>
### 组合入口

`UiTabContainer::add_tab(label, page)` 接收页面所有权并返回 `UiTabAddResult`，先检查其 `added` 字段。失败结果中的 `rejected_page` 若非空，调用方可从中收回页面；不能假定所有失败都归还非空对象，例如传入空页面时两者均为空／假。`remove_tab(index)` 返回已移除页面的所有权，越界时返回空。因此，失败或移除后仍需明确谁负责持有页面。

<a id="tabs-单独管理按钮和页面"></a>
### 单独管理按钮和页面

`UiTabBar` 管理页签按钮，提供 `add_tab`、`extract_tab`、`clear_tabs`、焦点索引、选中索引及相应回调。`UiTabView` 持有页面，提供 `add_page`、`extract_page`、`clear_pages`、`set_selected_index` 和 `clear_selection`。

自定义组合时，需要自行协调按钮选择与页面选择，不能只切换其中一侧。
