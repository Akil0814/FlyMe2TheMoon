# 文本、输入与数值展示

本页区分内容来源、排版测量、文字编辑与数值显示。业务数据由游戏提供，字体与翻译通过现有服务解析。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="text-content"></a>
## 内容来源与测量

使用 `ui_text_key("path")` 指定本地化键，使用 `ui_raw_text("...")` 提供原始文字，两者都生成 `UiTextContent`。`elysia::typography::UiTypographyRole` 选择字体、字号和默认对齐，`TextHorizontalAlign` 与 `TextVerticalAlign` 控制文字位置。

`content_extent()` 是给父布局的测量结果。可换行正文的高度依赖当前宽度，宽度变化后应重新布局；字体大小和布局占位不能由字符串长度简单推断。

<a id="ui-label"></a>
## UiLabel：单行文本

`UiLabel` 显示单行文字，适合标题、按钮说明和短标签。头文件为 [ui_label.h](../../../../engine/ui/widgets/label/ui_label.h)。

构造时或通过 `set_text_content` 提供 `UiTextContent`。`set_horizontal_align`、`set_vertical_align`、`set_text_fit_mode`、`set_typography_role` 和 `set_padding` 配置排版。

需要透明度动画时，可使用继承这些接口的 `UiBlinkLabel`、`UiFadeLabel` 或 `UiPulseLabel`，再通过 `configure_playback`、`play` 和 `set_on_end` 配置播放。文本与字体的共同规则见[文本专题](#text-content)。

<a id="ui-text-block"></a>
## UiTextBlock：多行文本

`UiTextBlock` 根据可用宽度显示可换行文字，适合说明正文和长消息。头文件为 [ui_text_block.h](../../../../engine/ui/widgets/text/ui_text_block.h)。

`set_text_content` 与 `clear_text` 管理内容，`set_typography_role`、`set_horizontal_align` 和 `set_padding` 配置测量与绘制。

`content_extent()` 使用当前宽度测量换行后的高度。父容器改变宽度后，需要重新计算布局，否则旧高度无法准确容纳文字。详见[文本与排版](#text-content)。

<a id="ui-text-input"></a>
## UiTextInput：单行文字输入

`UiTextInput` 处理单行 UTF-8 文本与输入法编辑。头文件为 [ui_text_input.h](../../../../engine/ui/widgets/ui_text_input.h)。

使用 `set_text`、`text` 和 `clear_text` 管理内容，使用 `set_placeholder_content` 和 `set_max_length` 设置提示与长度约束。长度以 UTF-8 码点数计，`nullopt` 表示不限；缩短上限会裁剪已有内容。光标和输入法组合文字属于内部编辑状态，没有公开的直接写入接口。

`set_on_text_changed` 和 `set_on_submit` 的参数为 `std::string_view`，只在回调执行期间有效。若要在回调结束后保留文字，必须复制为 `std::string`。程序调用 `set_text`、`clear_text` 或修改长度上限也会在最终文本改变时通知，不是静默赋值。键盘捕获规则见[输入捕获](input-and-focus.md#capture-and-cancel)。

<a id="value-display"></a>
## 数值展示控件：UiBar 与 UiNumber

`UiBar` 用填充比例表达数值，`UiNumber` 将数值排成文字。它们只呈现调用方提供的数据，不自行计算生命值、冷却或其他业务状态。

相关头文件：[ui_bar.h](../../../../engine/ui/widgets/ui_bar.h)、[ui_number.h](../../../../engine/ui/widgets/number/ui_number.h)。

<a id="value-display-uibar"></a>
### UiBar

`set_range(min, max)` 加 `set_value(value)` 将数值映射为填充比例；`set_ratio(ratio)` 直接设定 `[0, 1]` 范围的填充比例。`min_value`、`max_value`、`value`、`ratio` 查询最终状态。`set_fill_direction` 选择四个填充方向，`set_padding` 留出可绘制内边距。

<a id="value-display-uinumber"></a>
### UiNumber

<a id="value-display-数值精确整数与缺失值"></a>
#### 数值、精确整数与缺失值

`set_value(double)` 更新浮点数，`set_integer_value(std::int64_t)` 完整保留有符号 64 位整数的每一位。积分或计数可能超过 `double` 能精确表达的整数范围时，应直接使用整数接口，避免先转换为 `double`。

`integer_value()` 只在整数赋值状态返回精确值；即使浮点赋值是 `12.0`，它也返回空。`value()` 提供浮点视图，读取大整数时可能丢失精度，因此不能用它往返保存精确整数。

`clear_value()` 表示数值缺失，显示 `--`，省略小数与后缀。`set_value` 收到 NaN 或正负无穷时也进入这一状态。用 `has_value()` 区分缺失值与真实零：缺失时 `value()` 返回 `0`、`integer_value()` 返回空。再次赋予有限浮点数或整数即可恢复显示；构造和 `reset()` 后均为浮点零。

`set_decimal_places`、`set_trim_trailing_zeros`、`set_keep_decimal_point`、`set_suffix` 管理文本格式，`formatted_text()` 返回最终字符串。整数同样遵守这些设置，例如保留两位小数时 `42` 可显示为 `42.00`。百分号只追加 `%`，调用方负责把比例换算为百分数。数字始终使用 ASCII 数字和小数点，不随全局 locale 改变；当前整数接口不覆盖完整的 `uint64_t` 范围。

<a id="value-display-字形尺寸与区域适配"></a>
#### 字形尺寸与区域适配

`set_digit_spacing`、`set_fixed_glyph_advance`、`set_target_height` 管理字形几何；对应 `clear_*` 移除可选约束。固定前进距离控制每个字形占用的前进空间，相邻起点之间还会加上设定字距；前进距离可以小于字形宽度并产生重叠。

`set_shrink_to_fit(true)` 把整串字形等比缩小到 `set_padding` 后的可用宽高内，字距与固定前进距离也随之缩放。它只缩小，不放大；显式设置的目标高度是缩放前的高度。对齐使用缩放后的排版范围，控件的布局占位不变。固定前进距离较大时，最后一个字形后仍保留该列的前进空间。

缩小默认关闭，`reset()` 也会恢复关闭。关闭时保留原有字形尺寸，内容可能超过控件边界。开启后，位数增加可能让整串数字变小，因此固定前进距离不能保证不同数值之间的最终屏幕列宽始终一致。

目标高度和区域适配影响呈现几何，不改变数字来源或精度。字形缓存与命令生成方式见[布局与呈现架构](../../architecture/subsystems/ui/README.md#layout-presentation)。
