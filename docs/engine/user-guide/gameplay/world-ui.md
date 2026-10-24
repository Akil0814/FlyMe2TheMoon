# 世界空间 Gameplay UI

本文说明名字牌、世界血条和气泡如何与游戏对象组合。它们随世界相机投影，适合角色附近的提示；屏幕菜单和 HUD 仍使用普通 UI。下面的完整对象类型持有名字牌；把它加入已完成初始化的场景即可提交文字。示例省略角色贴图绘制。

`engine/gameplay/ui/` 提供 `elysia::gameplay::ui::WorldText`、`WorldBar` 和`SpeechBubble`。它们是由游戏对象持有的普通构件，输出世界空间 `RenderCommand`。位置、尺寸、尾巴和描边一起接受场景当前展示相机的投影，绘制顺序属于提交命令的`GameObject`。对象销毁时其成员随之释放，无须向场景注册或注销这些构件。

## 文字

```cpp
#include "engine/core/game_object.h"
#include "engine/gameplay/ui/world_text.h"

class NamedActor final : public elysia::core::GameObject
{
public:
    NamedActor() : GameObject(elysia::core::DepthLayer::Character)
    {
        set_size({32.0f, 48.0f});
        _nameplate.set_raw_text("Player");
        _nameplate.set_world_units_per_pixel(0.3f);
        _nameplate.set_color({220, 240, 255, 255});
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& commands) const override
    {
        // render_rect 跟随呈现姿态；底边中心锚点把名字放到对象上方。
        const elysia::core::Rect visual = render_rect();
        _nameplate.submit_render_commands(commands,
            {visual.center().x, visual.top() - 10.0f}, {0.5f, 1.0f});
    }

private:
    elysia::gameplay::ui::WorldText _nameplate;
};
```

最后一个参数是文字自身的归一化锚点；默认 `{0,0}` 将传入位置作为左上角，`{0.5,1}` 将文字底边中心放在该位置。`content_size()` 返回当前文字的世界尺寸。锚点应从对象的 `render_rect()` 取得，以跟随物理呈现插值。

`set_text_key` 使用当前语言查找翻译，`set_raw_text` 接收 UTF-8 原文；也可以`set_text_content(UiTextContent)`。空内容不产生命令。文字复用现有 UI 字体角色和来源，通过 `set_typography_role` 和 `set_font_source_override` 选择。

`set_world_units_per_pixel` 设置源文字纹理每像素对应的世界单位，默认 1，必须为有限正数；无效设置保留原值。`set_max_width` 的单位也是世界单位，默认 0 表示不自动换行，负值归一为 0。构件按比例将宽度转换为至少 1 像素的换行宽度，显式换行保留。相机缩放不会重新生成字体纹理。`set_visible` 控制绘制，尺寸测量不受可见性影响。

`Tint` 是默认着色策略，使用白色纹理和命令颜色调制；`Baked` 使用指定 RGB 的纹理。两种策略都把透明度放在命令中，单独改变透明度不会重建纹理。

翻译资源键每次提交时获取共享缓存纹理，因此能响应语言切换和缓存清理。原文纹理由构件独占；内容、语言、字体代次、字体角色／来源、换行像素宽度或烘焙 RGB变化时重建，位置或 Tint 颜色变化时复用。改为空内容或翻译资源键时释放原文纹理。纹理获取失败时跳过文字并在后续调用重试。

渲染命令只借用纹理，必须在修改／销毁构件或清理本地化缓存之前执行。场景的常规更新、收集命令、执行命令流程满足此要求。构件须在其 SDL 渲染器关闭之前销毁。

## 数值条

`WorldBar` 使用 `set_range(min,max)`、`set_value(value)` 或 `set_ratio(ratio)` 更新状态。默认范围 `[0,1]`，数值为 0。数值和比例会夹在范围内；倒置范围与非有限输入被忽略，相等上下界产生空条。四种 `WorldBarFillDirection` 指定固定边；`WorldBarStyle` 设置背景、填充、边框颜色和世界单位边框宽度，宽度 0 关闭边框。

调用 `submit_render_commands(commands, world_rect)` 按背景、填充、边框的顺序绘制。隐藏构件或空矩形不绘制。游戏层负责把血量等业务数值送入构件。

## 被动气泡

通过 `SpeechBubble::text()` 配置其 `WorldText`。默认字体角色为 `DialogBody`，最大文字宽度 160 世界单位。`SpeechBubbleStyle` 提供背景、边框、内边距和尾巴尺寸。`body_rect(tip)` 返回文字尺寸加内边距形成的矩形，底边位于尾巴尖端上方。`submit_render_commands(commands, tip)` 把尾巴尖端放在游戏层给出的世界位置，依次绘制背景、尾巴、连续边框和文字。空文字或隐藏状态不产生气泡。

气泡正常随世界出屏裁切，不自动翻转或挪回屏内；输入、距离判断、对话推进、持续时间和分支选择均由游戏层决定。

## 战斗示例与验证

战斗示例 `BlockCombatActor` 持有这三个构件。名字牌使用本地化资源键和预着色，世界血条替代原先手写矩形；受击后游戏逻辑显示气泡 1.5 秒，暂停时计时随对象暂停。显示锚点均由角色 `render_rect()` 计算，屏幕 HUD 仍使用原有 UI。

`world_ui_tests` 验证纹理复用与失效、着色、数值边界、气泡布局与绘制顺序，以及相机投影和 SDL 软件渲染。实际画面可在物理／战斗示例的自由游玩模式查看。


- [屏幕 UI](../ui/README.md#create-window)、[相机](../systems/camera.md)、[返回使用指南](../README.md)
