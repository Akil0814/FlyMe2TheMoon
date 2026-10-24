# UI 接口参考

本目录按主题维护公开契约。先按[使用指南](../../user-guide/ui/README.md#create-window)完成界面接入，再按任务或类名查询接口；内部执行流程见[UI 架构](../../architecture/subsystems/ui/README.md#element-tree)。

## 按任务查阅

| 主题 | 查询内容 |
| --- | --- |
| [元素与布局](elements-and-layout.md#ui-element) | 几何、所有权、容器选择与固定槽位 |
| [输入与焦点](input-and-focus.md#navigation) | 导航、捕获、取消与扩展接口 |
| [按钮与选择](controls-and-selection.md#callbacks) | 按钮、HUD、选择组、调值和页签 |
| [文本与数值](text-and-values.md#text-content) | 内容来源、文字输入、测量与精确数值 |
| [图像与动画](media-and-animation.md#media-controls) | 纹理、帧动画、位移和透明度 |
| [窗口与弹窗](windows-and-popups.md#surface-contract) | 窗口表面、下拉列表、对话框和提示 |
| [样式与主题](style-and-theme.md#style-contract) | 覆盖规则、主题注册与背景角色 |
| [设置面板](settings-panel.md#settings-panel) | 可见性、草稿、候选项与保存接入 |

## 按类名查阅

同一主题内先说明共同契约，再说明具体类型；下列链接直接定位到类型所在章节。枚举、配置和回调结果随所属类型说明。

| 类型 | 位置 |
| --- | --- |
| `UiElement` | [查看契约](elements-and-layout.md#ui-element) |
| `UiChildHost` | [查看契约](elements-and-layout.md#ui-child-host) |
| `UiLayoutChildOptions`、`UiLayoutAnchor`、`UiLayoutPadding`、`UiLayoutMargin`、`UiLayoutAlign` | [查看契约](elements-and-layout.md#fixed-layout) |
| `UiPanel` | [查看契约](elements-and-layout.md#ui-panel) |
| `UiListContainer` | [查看契约](elements-and-layout.md#ui-list-container) |
| `UiGridContainer` | [查看契约](elements-and-layout.md#ui-grid-container) |
| `UiScrollContainer` | [查看契约](elements-and-layout.md#ui-scroll-container) |
| `UiChromeContainer` | [查看契约](elements-and-layout.md#ui-chrome-container) |
| `UiControl` | [查看契约](input-and-focus.md#ui-control) |
| `UiFocusScope` | [查看契约](input-and-focus.md#ui-focus-scope) |
| `UiControlFocusScopeHost` | [查看契约](input-and-focus.md#ui-control-focus-scope-host) |
| `UiInputRouter`、`UiInputState`、`UiGamepadScrollSynthesizer`、`UiScrollState` | [查看契约](input-and-focus.md#input-runtime) |
| `UiAction`、`UiInputEvent` | [查看契约](input-and-focus.md#input-runtime) |
| `UiInteractionMode` | [查看契约](input-and-focus.md#capture-and-cancel) |
| `UiButton` | [查看契约](controls-and-selection.md#ui-button) |
| `UiActionButton` | [查看契约](controls-and-selection.md#ui-action-button) |
| `UiCheckbox`、`UiRadioButton` | [查看契约](controls-and-selection.md#selection-controls) |
| `UiButtonGroup`、`UiRadioGroup` | [查看契约](controls-and-selection.md#selection-groups) |
| `UiDragHandle`、`UiSlider` | [查看契约](controls-and-selection.md#drag-value-controls) |
| `UiLabeledCheckbox`、`UiLabeledRadioButton` | [查看契约](controls-and-selection.md#labeled-controls) |
| `UiOptionSelector`、`UiOptionSelectorOrientation`、`UiOptionSelectorOption` | [查看契约](controls-and-selection.md#ui-option-selector) |
| `UiTabBar`、`UiTabView`、`UiTabContainer` | [查看契约](controls-and-selection.md#tabs) |
| `UiTextContent`、`UiTypographyRole` | [查看契约](text-and-values.md#text-content) |
| `UiLabel` | [查看契约](text-and-values.md#ui-label) |
| `UiTextBlock` | [查看契约](text-and-values.md#ui-text-block) |
| `UiTextInput` | [查看契约](text-and-values.md#ui-text-input) |
| `UiBar`、`UiNumber` | [查看契约](text-and-values.md#value-display) |
| `UiImage`、`UiAnimation` | [查看契约](media-and-animation.md#media-controls) |
| `UiFadeLabel`、`UiBlinkLabel`、`UiPulseLabel`、`UiFadeImage`、`UiBlinkImage`、`UiPulseImage` | [查看契约](media-and-animation.md#opacity-variants) |
| `UiTranslationAnimationPlayer`、`UiOpacityFadeCore`、`UiOpacityBlinkCore`、`UiOpacityPulseCore` | [查看契约](media-and-animation.md#animation-primitives) |
| `UiWindow` | [查看契约](windows-and-popups.md#ui-window) |
| `UiOverlayOptions`、`UiOverlayPlacement`、`UiOverlayTransition` | [查看契约](windows-and-popups.md#uioverlayoptions放置与关闭规则) |
| `UiTransientPopup` | [查看契约](windows-and-popups.md#surface-contract) |
| `UiDialog`、`UiConfirmationDialog` | [查看契约](windows-and-popups.md#dialogs) |
| `UiDropdown` | [查看契约](windows-and-popups.md#ui-dropdown) |
| `UiTooltip` | [查看契约](windows-and-popups.md#ui-tooltip) |
| `UiThemeManager`、`UiThemeRegistration`、`UiThemeDefaults` | [查看契约](style-and-theme.md#style-contract) |
| `UiStyleState`、`UiThemeStyleResolver` | [查看契约](style-and-theme.md#theme-runtime) |
| `SettingsPanel`、`SettingsPanelDraft`、`SettingsPanelOptions`、`SettingsPanelVisibility` | [查看契约](settings-panel.md#settings-panel) |
