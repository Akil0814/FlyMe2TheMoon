# 元素、所有权与布局

本页说明 UI 树的共同接口，以及面板、列表、网格、滚动区和标题栏容器的布局差异。

需要完整接入步骤时阅读[UI 使用指南](../../user-guide/ui/README.md#create-window)；按类名查找其他主题见[UI 参考索引](README.md#按类名查阅)。

<a id="ui-element"></a>
## UiElement：几何、状态与生命周期

`UiElement` 提供几何、状态和呈现接口。布局尺寸与实际显示偏移是不同概念，使用前应先确认需要读写哪一种状态。源码见 [ui_element.h](../../../../engine/ui/core/ui_element.h)。

- `set_screen_rect`、`set_position`、`set_size`、`set_center` 写入实际布局几何；`screen_rect`、`position`、`size`、`center` 读取它。
- `content_extent()` 是向父布局报告的所需的布局尺寸；`size()` 是当前已分配几何，不能作为父布局测量的替代来源。
- `set_visible`、`set_active`、`set_opacity`、`set_order` 控制显示、参与性、透明度和渲染顺序。
- `reset()` 恢复对象状态；对象由 `std::unique_ptr` 拥有时不要保留其销毁后的裸指针。
- `bind_translation_animation`、`play_translation_animation`、`stop_translation_animation`、`active_translation_animation` 管理呈现位移；它不改变布局矩形。

以下局部片段假设 `window` 是已登记到场景的窗口，控件头文件已包含；返回指针只在窗口拥有该元素时可用。

```cpp
elysia::ui::UiButton* button = window.create_child<elysia::ui::UiButton>(elysia::core::Rect{0, 0, 180, 44});
button->set_visible(true);
button->set_opacity(220);
```

构造时可提供矩形、位置与尺寸，或中心与尺寸。自定义元素重写 `submit_ui_render_commands` 与 `content_extent`，分别提供绘制命令和测量结果。

`screen_rect()` 是布局矩形，`presentation_screen_rect()` 包含祖先的呈现位移。指针命中还需通过 `presentation_to_layout_point()` 转换到布局空间；不能把有动画的显示位置直接当成布局坐标。

<a id="ownership"></a>
## 子树所有权与布局测量

本文说明容器如何持有子元素，以及如何为它们分配位置和尺寸。`UiChildHost`、列表、网格、面板、滚动容器和窗口都遵循这些共同规则。

<a id="ownership-谁持有子元素"></a>
### 谁持有子元素

`add_child(std::unique_ptr<UiElement>, UiLayoutChildOptions)` 接管传入对象，`create_child<T>` 则先创建对象再交给容器。返回的 `UiElement*` 只是借用指针，不负责释放对象。

`extract_child` 将所有权移出容器；`clear_children` 销毁全部子元素。`child_at` 只提供访问入口。取出对象后，原指针是否仍有效取决于新的所有者，而不是它是否仍位于原容器。

<a id="ownership-如何分配尺寸"></a>
### 如何分配尺寸

子元素通过 `content_extent()` 报告所需尺寸，父布局据此分配实际几何。需要固定布局槽位时，在 `UiLayoutChildOptions` 中启用 `_use_size_override` 并提供 `_size_override`。

`UiLayoutAnchor`、`UiLayoutPadding`、`UiLayoutMargin` 和 `UiLayoutAlign` 定义在 [ui_layout_types.h](../../../../engine/ui/layout/ui_layout_types.h)。列表的 `set_cross_align` 决定子元素在非主轴上的对齐方式。

修改 `set_padding` 或 `set_child_layout_options` 会使布局失效；`set_clip_children` 仅修改渲染裁剪开关，不触发布局重建。不要通过不断改写子元素矩形来对抗父布局；应修改父布局使用的选项。

<a id="ui-child-host"></a>
## UiChildHost：容器操作

[`UiChildHost`](../../../../engine/ui/core/ui_child_host.h) 提供 `add_child`、`create_child`、`extract_child`、`clear_children`、`child_count`、`child_at`、`move_child`、`set_child_layout_options`、`set_padding`、`set_clip_children`、`mark_layout_dirty` 和 `update_layout_if_dirty`。

`set_child_layout_options(index, options)` 对越界索引不做修改。读取刚修改后的子项矩形前，可主动 `update_layout_if_dirty()`。布局用 `content_extent()` 测量内容，`screen_rect()` 和 `size()` 表示已分配几何；不要用两者互相替代。

`child_at`、`child_layout_options` 和 `extract_child` 对越界索引返回空指针。`move_child(from, to)` 对越界索引返回 `false`，两个索引相同且有效时返回 `true`，其余成功移动也返回 `true`。

`set_content`、`set_body` 和 `add_tab` 同样接管传入的智能指针。下面的局部片段假设已包含列表、按钮和智能指针头文件，`rect`、`button_rect` 由调用方提供：

```cpp
auto list = std::make_unique<elysia::ui::UiListContainer>(rect);
auto row = std::make_unique<elysia::ui::UiButton>(button_rect);
elysia::ui::UiButton* row_ptr = row.get();
list->add_back(std::move(row)); // list 现在拥有 row
row_ptr->set_enabled(false);    // 仅在 list 仍拥有它时有效
```

需要取回对象时使用 `extract_child`、`extract_page`、`extract_tab` 或 `release_content`；不要对借用指针调用 `delete`。

<a id="fixed-layout"></a>
### 布局选项与固定槽位

`UiLayoutChildOptions` 使用以下字段：

- `_anchor`：九宫格锚点；用于 `UiPanel` 和基础锚点布局。
- `_margin`：围绕子项的额外偏移/留白。
- `_size_override` + `_use_size_override`：明确指定子元素槽位尺寸。只有设置布尔开关才生效，默认应消费 `content_extent()`。
- `_cross_align` / `_use_custom_cross_align` / `_fill_cross_axis`：供列表等沿主轴堆叠的容器决定交叉轴位置和拉伸。
- 容器的 `UiLayoutPadding`：先由 `screen_rect` 扣出 `content_rect()`，再给布局算法使用。

列表未提供尺寸覆盖时，主轴使用 `content_extent()`。以下片段假设 `host` 是已有容器，`preview` 是待移交的 `unique_ptr<UiElement>`；显式开启尺寸覆盖后，父布局为它保留固定槽位：

```cpp
elysia::ui::UiLayoutChildOptions fixed{};
fixed._use_size_override = true;
fixed._size_override = {320.0f, 180.0f};
host.add_child(std::move(preview), fixed);
```

`set_cross_align(Start)` 适合表单和窄控件；默认 Center 适合菜单。父宽、内边距、方向、交叉轴对齐或子元素选项改变后，调用路径会使布局失效并在下一次更新或绘制时重新测量。

<a id="ui-panel"></a>
## UiPanel：面板容器

`UiPanel` 为子元素提供背景、边框和方向性的焦点关系。头文件为 [ui_panel.h](../../../../engine/ui/containers/ui_panel.h)。

`add_child(child, UiPanelInsertDirection)` 根据 `Up`、`Down`、`Left` 或 `Right` 建立相邻焦点。传入 `UiLayoutChildOptions` 的重载则使用锚点布局；添加对象会转移其所有权。

`UiPanelStyle` 描述样式，`UiPanelVisualRole` 选择主题角色。局部覆盖的共同语义见[样式专题](style-and-theme.md#style-contract)。

<a id="ui-list-container"></a>
## UiListContainer：列表容器

头文件：[ui_list_container.h](../../../../engine/ui/containers/ui_list_container.h)。沿主轴排列子元素并提供焦点导航，适合菜单和表单。

<a id="ui-list-container-基本配置"></a>
### 基本配置

`add_front`/`add_back` 添加子元素；`set_direction` 选择 Vertical/Horizontal；`set_cross_align` 选择 Start/Center/End；`set_item_spacing` 设置间距。未设置尺寸覆盖的子元素主轴尺寸来自 `content_extent()`。

也可以用于分组正文。需要固定子项槽位时，按[布局选项](#fixed-layout)传入尺寸覆盖。

<a id="ui-list-container-尺寸与对齐"></a>
### 尺寸与对齐

纵向列表使用子元素所需高度作为主轴长度；横向列表使用所需宽度。交叉轴默认 Center，`Start` 会将表单项贴到内容矩形起点，`End` 贴到终点。`set_padding` 来自 `UiChildHost`，会同时影响内容测量尺寸和子元素起点。

以下局部片段配置已有的 `UiListContainer list`：

```cpp
list.set_direction(elysia::ui::UiListDirection::Vertical);
list.set_cross_align(elysia::ui::UiLayoutAlign::Start);
list.set_item_spacing(8.0f);
```

<a id="ui-grid-container"></a>
## UiGridContainer：网格容器

`UiGridContainer` 按网格排列子元素，并为它们建立焦点关系。它适合物品格或多列选择界面。头文件为 [ui_grid_container.h](../../../../engine/ui/containers/ui_grid_container.h)。

通过 `add_child` 接管子元素，使用 `set_column_count` 设置列数、`set_cell_spacing` 设置间距、`set_fill_by_row` 选择填充顺序。返回的子项指针只提供访问能力。

`content_extent` 综合显式尺寸与内容所需尺寸，返回较大的结果。父布局和固定槽位规则见[子项与布局](#ownership)。

<a id="ui-scroll-container"></a>
## UiScrollContainer：滚动容器

头文件：[ui_scroll_container.h](../../../../engine/ui/containers/ui_scroll_container.h)。为一个内容子元素提供可滚动视口，适合长列表和说明正文。

<a id="ui-scroll-container-内容与滚动"></a>
### 内容与滚动

`set_content` 替换并接管内容；`clear_content` 移除。`set_scroll_axis`、`set_scrollbar_visibility`、`set_scroll_offset`、`set_scroll_step` 及 x/y 变体控制滚动。`scroll_offset`、`max_scroll_offset`、`viewport_rect`、`content_rect` 查询结果。

内容元素的 `content_extent()` 决定可滚动范围；不要把固定大高度当作内容测量替代品。

<a id="ui-scroll-container-行为细节"></a>
### 行为细节

`UiScrollAxis::Auto` 会根据内容是否超出视口决定可滚动轴。`UiScrollBarVisibility::Auto` 仅在对应轴有溢出内容时显示；`Hidden` 不显示滚动条但不禁用滚动。`set_scroll_offset` 会限制在合法范围内，最大值为 `max_scroll_offset`，所以可安全传入越界值。

`set_content` 会替换旧内容元素并销毁它；若需保留旧对象，先调用 `extract_child(0)` 或在替换前转移所有权。

<a id="ui-chrome-container"></a>
## UiChromeContainer：标题栏与主体容器

`UiChromeContainer` 将固定标题栏与主体内容组合起来。标题栏具有左侧操作、标题和右侧操作三个位置，主体只有一个内容槽位。头文件为 [ui_chrome_container.h](../../../../engine/ui/containers/ui_chrome_container.h)。

通过 `add_left_action`、`add_title_child`、`add_right_action` 添加标题栏元素，使用 `set_body` 接管主体；对应的 `clear_*` 接口清空内容。`set_header_visible`、`set_header_height`、`set_header_padding` 和 `set_body_padding` 配置结构。

`content_extent()` 包含标题栏与主体所需尺寸，因此放入列表后可由父布局据此分配空间。设置主体时应留意旧对象的生命周期，详见[所有权规则](#ownership)。
