# UI 使用指南

本指南用一个带确认弹窗的退出菜单串起窗口、布局、文本、主题和焦点。UI 元素由场景和容器持有，标准场景负责输入、更新和绘制；游戏只组装界面并处理业务回调。

按下面的步骤理解接入关系，最后的完整场景可以按[游戏初始化](../game-initialization.md)注册。具体参数和控件差异集中在[UI 参考](../../reference/ui/README.md)。

<a id="create-window"></a>
## 创建窗口

在 `on_enter` 中用 `create_and_add_object<UiWindow>` 创建场景根窗口。它属于 UI 树，与世界中的 `GameObject` 分开管理。窗口坐标使用 UI 空间，高 `order` 在同级中后绘制、先接收输入。

场景已把原始输入转换成 UI 输入，并执行更新和绘制命令；不要在派生场景里再调用一遍路由或绘制。父布局分配 `screen_rect()`；存在呈现位移时，命中位置还要经过呈现坐标转换，普通控件已处理这一步。

<a id="layout"></a>
## 组织内容与布局

示例窗口持有一个竖向 `UiListContainer`，列表持有退出按钮。`create_child` 创建并移交所有权，`add_child` 接收 `unique_ptr`；返回的裸指针只是借用，不能自行释放。移动子项时先 `extract_child`，再移交新容器。

用列表排列菜单或操作条，用网格排列规则图标，用面板组合自由布局。单项锚点、边距和尺寸覆盖通过 `UiLayoutChildOptions` 配置；其现有字段名包括 `._anchor` 和 `._size_override`。不要通过每帧改写子项矩形对抗父布局。

需要滚动时，先把多项放入列表或网格，再用 `UiScrollContainer::set_content` 接管这个内容节点。尺寸测量与各容器的选择见[元素与布局](../../reference/ui/elements-and-layout.md)。

<a id="text-and-theme"></a>
## 设置文本、主题与局部样式

示例用 `ui_raw_text` 显示固定文字。正式项目可改用 `ui_text_key`，并在语言文件中提供对应翻译；用户输入仍应使用原文来源。名字、消息和按钮标题都通过 `UiTextContent` 表达来源。

把 `UiThemeManager` 和 `register_root` 返回的 `UiThemeRegistration` 保存为宿主成员，才能让窗口在后续切换主题时继续更新。局部变量离开作用域就会解除注册。

按钮的 Primary 角色表达主要操作，局部样式只覆盖需要固定的字段。示例只固定圆角半径，其余颜色继续来自主题。`set_style_overrides` 替换整份覆盖配置；清除覆盖后使用当前基础样式。完整规则见[样式与主题](../../reference/ui/style-and-theme.md)。

<a id="focus"></a>
## 配置键盘与手柄焦点

将列表注册为窗口焦点区域，再调用 `focus_first_available_scope()` 选中可用目标。列表内部维护按钮导航；多个区域之间用 `UiFocusScopeNeighbors` 配置方向关系。

普通 `Scene` 默认使用 Navigation，示例无需重复设置。`GameplayScene` 默认使用 Pointer，打开交互菜单时应切到 Navigation，关闭后恢复业务所需模式。UI 手柄选择与玩家设备归属独立，见[场景菜单与玩法输入](../gameplay/scene.md#菜单与玩法输入)。

临时隐藏或停用区域会使其暂不可用，不等于删除焦点注册；重新显示后可通过现有焦点恢复流程导航。销毁或移出窗口子树才会清理失效注册。不要长期保存失效的 `focused_target()` 指针。

<a id="popups"></a>
## 打开确认弹窗

把 `UiConfirmationDialog` 作为窗口的直接子元素创建，再检查 `register_with_window` 返回值。只有注册成功后才把按钮回调连接到 `open()`；确认时弹窗先关闭，再调用业务回调。示例的确认动作请求退出应用，取消则恢复菜单交互。

对话框放在嵌套面板中会注册失败。下拉列表的注册条件不同，可位于窗口的嵌套子树；提示则是被动显示层。三类表面的契约见[窗口与弹窗](../../reference/ui/windows-and-popups.md)。

<a id="cleanup"></a>
## 清理与复用

示例每次进入都重建菜单，退出和重置时先解除主题关联，再清理场景对象。窗口销毁时释放子树；无须逐个删除按钮和弹窗。

按钮借用弹窗，弹窗回调借用场景，所以它们必须保持共同的生命周期。若单独移除弹窗，需要先解除按钮里捕获它的回调。`reset()` 会清理对应控件的状态，不能假定旧回调、选择或子树仍然保留。

<a id="complete-example"></a>
## 完整菜单场景

下面是完整类型定义，不依赖仓库演示游戏的场景键或翻译数据。弹窗注册失败会抛出接入异常，由场景失败边界处理；不会继续把未注册元素当作覆盖层使用。

```cpp
#include "engine/scene/scene.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/style/ui_theme_manager.h"
#include <stdexcept>

class GuideMenuScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        clear_menu(); // 本例选择每次进入重建菜单。
        elysia::ui::UiWindow* window = create_and_add_object<elysia::ui::UiWindow>(
            elysia::core::Rect{40, 40, 600, 360}, 100);
        window->set_padding({24.0f, 24.0f, 24.0f, 24.0f});
        elysia::ui::UiListContainer* actions =
            window->create_child<elysia::ui::UiListContainer>(
                elysia::ui::UiLayoutChildOptions{},
                elysia::core::Rect{0, 0, 220, 200});
        actions->set_direction(elysia::ui::UiListDirection::Vertical);
        actions->set_item_spacing(12.0f);

        elysia::ui::UiConfirmationDialog* dialog =
            window->create_child<elysia::ui::UiConfirmationDialog>(
                elysia::ui::UiLayoutChildOptions{});
        dialog->set_config({
            .title = elysia::ui::ui_raw_text("Exit game"),
            .message = elysia::ui::ui_raw_text("Return to desktop?"),
            .confirm = elysia::ui::ui_raw_text("Exit"),
            .cancel = elysia::ui::ui_raw_text("Cancel")});
        // 弹窗必须先成为窗口的直接子元素，再注册覆盖层。
        if (!dialog->register_with_window(*window))
        {
            throw std::runtime_error("Failed to register exit dialog.");
        }
        dialog->set_on_confirm([this]
        {
            request_quit();
        });

        elysia::ui::UiButton* exit_button = actions->create_child<elysia::ui::UiButton>(
            elysia::ui::UiLayoutChildOptions{},
            elysia::core::Rect{0, 0, 180, 44},
            elysia::ui::UiButtonConfig{.content = elysia::ui::ui_raw_text("Exit")});
        // 回调借用同一窗口拥有的弹窗；本例总是一起销毁整棵菜单树。
        exit_button->set_on_click([dialog]
        {
            dialog->open();
        });
        exit_button->set_visual_role(elysia::ui::UiButtonVisualRole::Primary);
        elysia::ui::UiButtonStyleOverrides overrides = exit_button->style_overrides();
        overrides.chrome.corner_radius = 8.0f;
        exit_button->set_style_overrides(overrides);

        window->register_focus_scope(*actions);
        window->focus_first_available_scope();
        _themes.set_theme(elysia::ui::UiBuiltinTheme::ElysiaDark);
        _theme_registration = _themes.register_root(*window);
    }

    void on_exit() override
    {
        clear_menu();
    }

    void on_reset() override
    {
        clear_menu();
    }

private:
    void clear_menu()
    {
        // 先解除主题关联，再清理窗口及其子树。
        _theme_registration.reset();
        clear_scene_objects();
    }

    elysia::ui::UiThemeManager _themes;
    elysia::ui::UiThemeRegistration _theme_registration;
};
```

## 按任务继续阅读

| 任务 | 文档 |
| --- | --- |
| 查询元素、容器和固定槽位 | [元素与布局](../../reference/ui/elements-and-layout.md) |
| 查控件 API 或返回结果 | [按类名查阅](../../reference/ui/README.md#按类名查阅) |
| 显示角色名字、世界血条和气泡 | [世界空间 UI](../gameplay/world-ui.md) |
| 使用内置设置界面 | [设置场景](../builtin-scenes/settings.md) |
| 注册 ImGui 开发面板 | [开发面板](../tools/development-panels.md) |
| 切换语言与配置文字来源 | [本地化](../systems/localization.md) |
| 理解或扩展 UI 实现 | [UI 架构](../../architecture/subsystems/ui/README.md#element-tree) |

[返回使用指南](../README.md)
