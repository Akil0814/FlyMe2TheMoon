# 图像、动画与呈现效果

UI 动画只推进呈现状态，不代替布局，也不承担必须执行的游戏规则。普通界面使用已有组件；自定义元素才直接组合底层状态机。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="media-controls"></a>
## 媒体控件：UiImage 与 UiAnimation

图像控件显示已有纹理，动画控件播放已注册的动画资源。两者都负责界面呈现，不管理游戏规则。

相关头文件：[ui_image.h](../../../../engine/ui/widgets/image/ui_image.h)、[ui_animation.h](../../../../engine/ui/widgets/image/ui_animation.h)。

<a id="media-controls-uiimage"></a>
### UiImage

构造器接收借用的 `SDL_Texture*` 和矩形、位置或中心对应的几何。`set_texture` 可更换借用纹理；调用方负责纹理生命周期。`set_source_rect` 裁剪纹理空间中的子区域，`clear_source_rect` 恢复完整纹理。

<a id="media-controls-uianimation"></a>
### UiAnimation

`set_animation_key` 绑定游戏动画键，`set_engine_animation(BuiltinAnimationId)` 绑定引擎内置动画。两者成功时返回 `true`，重置动画到起点并开始播放；失败时返回 `false`，清空当前动画和键，不保留之前的画面。带动画键的构造器无法返回结果，需要检查失败时应显式调用绑定接口。源码见 [ui_animation.cpp](../../../../engine/ui/widgets/image/ui_animation.cpp)。

`set_loop` 覆盖资源定义中的循环设置，并在之后重新绑定资源时继续生效。`play` 从起点重新播放，`pause` 暂停，`resume` 继续；`reset` 同时重置元素状态和当前动画。未绑定动画时，`is_finished()` 为 `false`、`is_paused()` 为 `true`，不会提交绘制命令。`animation_key` 与 `is_looping` 可查询当前配置。

透明度动画图像见 [透明度动画组件](#opacity-variants)；它们继承 `UiImage` 的纹理与源矩形接口。

<a id="opacity-variants"></a>
## 透明度动画组件

这些组件将透明度状态机组合到文字或图像中，用于淡入淡出、闪烁和脉冲效果。播放推进由 UI 更新流程负责。

相关头文件：[ui_fade_label.h](../../../../engine/ui/widgets/label/ui_fade_label.h)、[ui_blink_label.h](../../../../engine/ui/widgets/label/ui_blink_label.h)、[ui_pulse_label.h](../../../../engine/ui/widgets/label/ui_pulse_label.h)、[ui_fade_image.h](../../../../engine/ui/widgets/image/ui_fade_image.h)、[ui_blink_image.h](../../../../engine/ui/widgets/image/ui_blink_image.h)、[ui_pulse_image.h](../../../../engine/ui/widgets/image/ui_pulse_image.h)。

三组 Label/Image 分别继承 `UiLabel`/`UiImage` 的全部公开 API，并额外提供相同流程：

1. `configure_playback(mode, ...)` 设置 Fade/Blink/Pulse 参数；
2. 可选 `set_on_end(callback)` 接收一次完成通知；
3. `play()` 重置并开始；
4. 容器每帧调用 `update(delta)`，组件将状态机计算的透明度写回自身。

`Fade` 适合单次进出，`Blink` 使用显示与隐藏的持续时间，`Pulse` 在最小与最大透明度间往返。Blink/Pulse 的可选循环次数默认 `nullopt`，表示不按次数结束；次数为 0 会在 `play()` 中立即完成。应先登记回调再开始播放。

完成后的对象寿命不同：`UiFadeLabel` 和 `UiFadeImage` 先调用 `destroy()` 标记销毁，再通知完成，随后会被所属容器清理；Blink/Pulse 组件只通知完成并保留对象。重复 `play()` 重置完成通知状态，但不会撤销 Fade 的销毁标记，不能借此复用已结束的 Fade 元素。

Label 版本的 `play()` 声明为 `noexcept`；同步完成通知中的异常不能逃出回调。需要报告失败时，由回调保存结果或交给宿主处理。完成通知的回调捕获仍须遵守宿主与控件的生命周期。

<a id="animation-primitives"></a>
## UI 动画基础状态机

本文面向编写自定义 UI 元素的开发者。基础状态机只计算位移或透明度，不绘制，也不会自动推进；普通界面优先使用已有的动画 Label 或 Image 组件。

相关头文件为 [ui_translation_animation_player.h](../../../../engine/ui/effects/ui_translation_animation_player.h)、[ui_opacity_blink_core.h](../../../../engine/ui/effects/ui_opacity_blink_core.h)、[ui_opacity_fade_core.h](../../../../engine/ui/effects/ui_opacity_fade_core.h) 和 [ui_opacity_pulse_core.h](../../../../engine/ui/effects/ui_opacity_pulse_core.h)。

<a id="animation-primitives-位移"></a>
### 位移

`UiTranslationAnimationPlayer` 通过 `bind(name, animation)` 登记命名动画，使用 `play(name)` 启动、`stop` 停止、`update(delta)` 推进。`remove` 和 `clear` 移除登记；`translation` 返回当前呈现偏移，供 `UiElement` 的呈现接口使用，不改写布局矩形。

空名称的 `bind` 不做修改，同名登记会替换定义。`play` 找不到名称时返回 `false` 并保留原播放状态；成功返回 `true`，非正时长直接到终点。`update` 返回可见位移是否改变。`stop` 停止播放但保留当前偏移；移除正在播放的定义也会停止，`clear` 才会同时清零偏移。播放完成后 `is_playing()` 为 `false`，`active_name()` 仍保留名称，直到停止或清空。

<a id="animation-primitives-透明度"></a>
### 透明度

三个透明度状态机通过 `configure_playback` 设置模式和时间，使用 `play` 开始、`update(delta)` 推进、`opacity` 读取当前值，并用 `is_finished` 查询完成状态。组合它们的元素负责将结果写入自身透明度，详见[透明度动画组件](#opacity-variants)。
