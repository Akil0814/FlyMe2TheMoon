# 设置面板

本页说明设置表单的配置契约；直接使用引擎设置场景时，先阅读[内置设置场景指南](../../user-guide/builtin-scenes/settings.md)。

<a id="settings-panel"></a>
## SettingsPanel：设置表单

头文件：[settings_panel.h](../../../../engine/ui/presets/settings_panel.h)。可复用的设置表单，组合窗口模式、分辨率、帧率上限、VSync、三路音量、语言、状态消息及 Save/Back 动作。

本文说明设置表单的组成、草稿与宿主的分工，以及嵌入窗口时的调用顺序。面板只编辑数据，宿主负责应用和保存。

面板由左对齐的固定标题、单个纵向滚动内容区、固定状态消息和底部右对齐的 Back/Save 组成。字段按 Display、Audio、General 的固定顺序分组，每组使用现有主题的面板底色，不覆盖主题颜色；音量百分比位于滑条右侧。标题、状态和操作按钮不随内容滚动。输入映射不属于当前预设，扩展设置流程由游戏决定。

<a id="settings-panel-组件可见性"></a>
### 组件可见性

`SettingsPanelVisibility` 控制是否创建各个内置字段，所有开关默认为 `true`。可见性在面板生命周期内不变；如果项目要更换组件集合，应重建面板。

```cpp
elysia::ui::SettingsPanelVisibility visibility{};
visibility.target_fps = false;
visibility.vsync = false;

auto panel = std::make_unique<elysia::ui::SettingsPanel>(
    panel_rect,
    visibility);
```

- 隐藏字段不创建控件，不参与布局、焦点导航或下拉控件弹出层注册。
- VSync 重启提示只在 VSync 字段可见时创建。
- `window_mode` 同时控制窗口模式和窗口尺寸两个字段的创建。无边框全屏时禁用窗口尺寸，保留上一次的窗口尺寸，切回窗口模式即可继续编辑。
- 只有一个非空区段时省略区段标题；有两个或以上非空区段时，每个非空区段都显示标题。
- 所有字段都隐藏时，滚动内容为空，Back/Save 仍可用，默认焦点从 Back 开始。
- 可见性只决定是否创建编辑控件，不会从 `SettingsPanelDraft` 删除字段。隐藏字段保留宿主写入的基线值，Save 仍一次提交完整草稿。

`SettingsScenePayload::visibility` 使项目可在进入内置设置场景时选择组件。仅设置 `return_route` 的旧调用保持全量显示；缓存场景再次进入且可见性变化时，场景会重建 UI。

<a id="settings-panel-数据与选项"></a>
### 数据与选项

- `SettingsPanelDraft` 是面板当前编辑的完整草稿；`set_draft` 写入并同步已创建的控件，`draft` 返回借用的只读引用。
- `SettingsPanelOptions` 提供窗口尺寸、帧率和语言候选项；`set_options` 会移除无效或重复值，并保留草稿中仍有效但未列出的当前值。可选的 `usable_window_size` 限制窗口尺寸，超出范围的旧值不会被重新加入；窗口字段可见时，超限草稿会改选列表中最大的可用尺寸，直到 Save 才由宿主应用。
- `make_settings_window_size_options(usable_size, current_size)` 返回不超过可用尺寸的预设，当前自定义尺寸符合限制时也会保留；没有尺寸可选时使用可用区域尺寸。内置场景每次进入时查询游戏窗口所在显示器的工作区，无法确定窗口所在显示器时回退到主显示器。
- `make_settings_target_fps_options(current_fps)` 返回 `30/60/120/240`，并确保包含当前合法帧率；144 FPS 不属于内置预设，但已有的 144 或其他自定义值不会丢失。
- FPS 上限允许超过显示器刷新率，不根据显示器刷新率过滤。
- `reset_navigation_state` 只将唯一滚动容器归顶，不修改草稿、候选项、可见性或回调。
- 面板只维护草稿，不应用或持久化设置。Save 回调接收当前 `SettingsPanelDraft`，宿主负责验证、应用和保存。

<a id="settings-panel-调用顺序"></a>
### 调用顺序

以下局部片段位于宿主成员函数中，矩形、选项和草稿已准备好；`apply_settings` 由游戏实现，负责处理应用与持久化失败。回调借用的宿主应活到面板销毁或回调解除。

```cpp
auto panel = std::make_unique<elysia::ui::SettingsPanel>(panel_rect);
elysia::ui::SettingsPanel* panel_ptr = panel.get();
panel_ptr->set_options(options);
panel_ptr->set_draft(draft);
// 宿主处理保存结果，再通过状态消息反馈给用户。
panel_ptr->set_on_save([this](const elysia::ui::SettingsPanelDraft& next)
{
    apply_settings(next);
});
window.add_child(std::move(panel));
panel_ptr->register_with_window(window);
```

已创建的窗口模式、窗口尺寸、帧率和语言下拉控件需要窗口级弹出层注册，因此必须在面板已由对应 `UiWindow` 拥有后调用 `register_with_window`；换窗口或提前移除时调用 `unregister_from_window`。隐藏的下拉控件不会注册。析构与 `reset` 会自动解除现有窗口注册。

`set_status_message(message, is_error)` 显示宿主提供的状态文本，`clear_status_message` 隐藏它。`set_on_back` 注册返回动作；和其他 `set_on_*` API 一样，后一次调用替换前一次回调。
