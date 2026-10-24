# 样式、主题与视觉角色

基础样式提供整套值，局部覆盖固定少数字段，视觉角色表达组件用途。主题管理器将这些约定应用于已注册的 UI 子树。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="style-contract"></a>
## 样式覆盖与主题寿命

样式决定组件如何呈现，主题提供一组协调的基础样式。局部覆盖用于表达调用方明确指定的差异；切换主题时，这些差异应继续保留。

<a id="style-contract-基础样式与局部覆盖"></a>
### 基础样式与局部覆盖

`set_base_style` 替换完整基础样式，`set_style_overrides` 替换只包含部分字段的覆盖配置。后者不是与旧覆盖增量合并。`style` 返回生效结果，`style_overrides` 返回当前覆盖，`has_style_overrides` 查询是否存在覆盖；`clear_style_overrides` 清除局部配置。

`set_visual_role` 选择组件的语义角色，例如主要操作按钮。角色交给主题解释，本身不持有颜色。

<a id="style-contract-主题注册的生命周期"></a>
### 主题注册的生命周期

`UiThemeManager::register_root` 注册一棵 UI 子树，并返回 `UiThemeRegistration`。调用方需要保存登记对象；同一管理器重复登记同一根时共享计数，最后一个登记对象释放才解除绑定。`registered()` 查询登记是否仍有效，`reset()` 主动释放本次登记；`unregister_root` 则解除整个根的登记。换用另一个管理器登记同一根时，原管理器的登记会失效。通过 `set_theme` 切换主题，或用 `reapply_theme` 重新计算。

`UiThemeDefaults` 保存进程级的构造默认主题。修改它只影响随后创建或主动重置的控件，不会刷新已有 UI。`UiThemeManager` 在构造时取得默认主题快照，此后独立管理注册的子树。

进程默认值为 `UiBuiltinTheme::BlueGlassMoon`。`UiThemeDefaults::set_builtin_theme` 对合法内置主题返回 `true`；无效枚举返回 `false`，保留原默认值。`current_builtin_theme` 查询枚举，`theme` 查询对应主题，定义见 [ui_theme_defaults.h](../../../../engine/ui/style/ui_theme_defaults.h)。

字段定义和视觉角色位于 `engine/ui/style/`。具体组件的样式配置见相应参考页，内部合成与遍历见[样式运行时](#theme-runtime)。

<a id="theme-example"></a>
## 保存主题注册

以下片段假设管理器、窗口和按钮仍存活，且已包含对应头文件。登记对象应由界面宿主保存，不能在初始化结束时就销毁。

```cpp
// 保存注册对象，直到界面不再需要响应主题变化。
elysia::ui::UiThemeRegistration registration = theme_manager.register_root(window);
theme_manager.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);

elysia::ui::UiButtonStyleOverrides overrides{};
overrides.chrome.corner_radius = 10.0f;
button.set_style_overrides(overrides);
```

<a id="theme-runtime"></a>
## 主题与样式扩展接口

- `UiStyleState<Style>`：组件内部合成基础样式与局部字段覆盖；调用 `reset`、`set_base_style`、`set_style_overrides`、`effective_style`、`clear_style_overrides`。
- `UiThemeStyleResolver`：用 `register_adapter` 注册类型适配器，再用 `apply` 将一个元素与 `UiTheme` 解析为样式及后续遍历策略；整棵子树的遍历由 `UiThemeManager` 负责。
- `UiThemeManager`：应用代码入口。保存 `register_root` 返回的 `UiThemeRegistration`；使用 `set_theme`、`current_theme`、`reapply_theme`。整棵根子树可用 `unregister_root` 或登记对象的 `reset` 解除绑定；`detach_subtree` 用于移除子树的主题上下文。

遍历、复合组件样式归属和类型适配的协作见[UI 架构](../../architecture/subsystems/ui/README.md#theme-flow)。

<a id="theme-backgrounds"></a>
## 内置主题的背景职责

七种主题共用背景映射规则，组件按用途选择角色，无需判断主题名称：

- `UiWindow` 与 `UiPanelVisualRole::Screen` 使用页面背景。
- `UiPanelVisualRole::Default`、`List` 与 `UiChromeContainer` 主体使用分区背景；Chrome 标题底板使用页面背景。
- `UiScrollContainer` 默认背景色为透明，显示宿主窗口、分区或弹出面板的底色。滚动条和边框仍使用当前主题颜色。
- `Dialog` 角色保留独立的原有背景；下拉菜单由不透明的 `List` 面板提供底色。

| 主题 | 页面背景 | 分区背景 |
| --- | --- | --- |
| BlueGlassMoon | `#08101C` | `#0C224E` |
| ElysiaLight | `#F7DFF6` | `#FDFAFC` |
| ElysiaDark | `#141D4C` | `#2E3D7C` |
| EvangelionUnit00 | `#DCEAF6` | `#F3F6FA` |
| EvangelionUnit01 | `#221635` | `#31214C` |
| EvangelionUnit02 | `#4D1118` | `#6C181F` |
| QuietSlate | `#20262E` | `#363F4A` |

背景映射只修改上述主题背景字段，不修改文字、控件交互状态或边框颜色。需要自有底色的滚动容器可用 `UiScrollContainerStyleOverrides::background_color` 显式覆盖；覆盖在运行时切换主题后仍保留。
