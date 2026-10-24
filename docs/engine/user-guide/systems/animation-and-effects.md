# 动画与特效

本文说明已加载的动画如何播放，以及特效如何使用注册定义提交绘制。资源配置、播放状态与游戏规则分别维护，避免在绘制阶段推进动画或发放效果。

先在[资源配置](../content/resources.md)中声明图集、动画和特效，再等待内容加载成功。游戏无需初始化或关闭动画、特效服务；根据表现的归属选择入口：

| 需求 | 入口与归属 |
| --- | --- |
| 对象自己播放动画 | `AnimationService::create_animation()` 返回由调用方持有、更新的播放器 |
| 世界中的命中特效或飘字 | `EffectService` 将特效对象同步加入活动场景，由场景更新与回收 |
| 闪白、黑幕或屏幕动画 | `EffectService` 返回屏幕特效句柄，退出所属场景时清除 |

## 播放对象动画

以下类可放入游戏头文件，在场景进入、资源加载成功后创建。`test.animation` 对应下文示例清单，图片需由项目提供。所属场景还须启用相机并将镜头对准对象，见[相机接入](camera.md#配置跟随)。

```cpp
#include "engine/animation/animation_service.h"
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/tools/logger.h"

class AnimatedItem final : public elysia::core::GameObject,
                           public elysia::core::Updatable
{
public:
    AnimatedItem() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({100, 100, 64, 64});
        // 对象独占动画播放实例；其引用的图集仍由资源系统持有。
        _animation = ELYSIA_ANIMATIONS->create_animation("test.animation");
        if (!_animation)
        {
            ELYSIA_LOG_ERROR("animation", "Missing test.animation");
            destroy();
        }
    }

    void update(double delta) override
    {
        // 播放状态只在更新阶段推进，绘制阶段只读取当前帧。
        if (_animation)
            _animation->update(scaled_delta(delta));
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out) const override
    {
        if (!_animation)
            return;
        elysia::core::RenderCommand command;
        // 当前帧无法生成有效绘制命令时跳过，不能提交未填写的命令。
        if (_animation->build_render_command(render_rect(), 0.0,
                elysia::core::SpriteFlip::None, command))
            out.push_back(command);
    }

private:
    std::unique_ptr<elysia::animation::Animation> _animation;
};
```

播放器归调用方所有，但内部 `Atlas` 和帧纹理为借用。资源重载前销毁或停用依赖旧资源的播放器；暂停播放器并不能让借用资源跨重载有效。

`set_loop` 设置循环，`set_interval_seconds` 设置帧间隔，`pause`/`resume` 暂停或继续，`reset` 从首帧重新开始并恢复计时。非循环播放结束时 is_finished 为 true，停留最后一帧；resume 不替代 reset。`set_on_finished` 接收无参数回调，回调中不要立即释放正在执行更新的播放器。

`current_frame` 和服务的 `find_definition` 都返回可空借用。`append_render_commands` 可附加颜色覆盖；返回 false 时不假设生成了命令，覆盖需要相应帧的 coverage mask。通常使用上面的 build_render_command 即可。

## 请求场景特效

以下辅助函数在活动场景的安全阶段调用，例如在 `on_enter`、`on_before_update` 或 `on_after_update` 中处理本帧积累的表现请求。特效对象通过同步 `add_object` 加入场景；不要在对象更新、UI 呈现动画、渲染、查询、物理模拟步的碰撞监听或对象特效完成回调中直接插入新特效，应先保存值请求。`on_after_update` 在物理和相机之后、标记对象回收之前执行。完整规则见[场景阶段表](../scene/scene.md#常用操作与扩展点)。

```cpp
#include "engine/effects/effect_service.h"
#include "engine/tools/logger.h"

void show_hit(elysia::core::Vector2 position)
{
    elysia::effects::AnimationEffectSpawnRequest request;
    request.effect_key = "effect.test";
    request.position = position;
    request.anchor = elysia::effects::EffectAnchor::Center;
    if (!ELYSIA_EFFECTS->request_animation_effect(request))
        ELYSIA_LOG_WARN("effects", "Hit effect was not created.");

    elysia::effects::FloatingNumberEffectSpawnRequest number;
    number.text = "120";
    number.position = position;
    number.target_height = 20.0f;
    number.lifetime_seconds = 0.6;
    if (!ELYSIA_EFFECTS->request_floating_number_effect(number))
        ELYSIA_LOG_WARN("effects", "Damage number was not created.");
}
```

动画特效 position 是选定 anchor 的世界位置；可覆盖 size、angle_degrees、flip，设置 start_delay_seconds。缺少活动场景、定义或创建失败返回 false。该接口立即加入对象，true 不表示已经播放结束。

特效非循环动画完成后调用 on_finished 并标记销毁；循环动画不会自然结束。服务不返回可取消句柄。需要主动管理的长期表现优先由自己的对象持有 `Animation`；不要假设退出场景会触发完成回调。场景缓存复用时，特效可能仍保留在原场景中。

on_started、on_finished 和 scheduled_callbacks 的回调参数是 `AnimationEffect`&；scheduled_callbacks 的 delay_seconds 相对于播放时间。回调捕获的数据必须覆盖实际执行期，不能把完成回调作为释放外部资源的唯一途径。

浮动数字不需要配置动画清单，但需要有效字体与渲染依赖。text 支持 `0–9`、`-`、`.`、`/`、`%`，不支持加号或普通文字；普通文字使用 UI 文本。position、字号高度、时间必须合法，lifetime_seconds 和 target_height 为正，time_scale 非负。effects 可组合线性/弧线 motion、scale、fade，其 time_range 使用 0–1 生命周期进度。没有设置这些效果时仍按寿命结束并销毁。其 on_finished 参数为 `FloatingNumberEffect`&。

## 全屏特效

`EffectService` 支持纯色、静态图片和动画屏幕特效。它们使用游戏逻辑视口的屏幕坐标，不受相机移动或缩放影响，也不覆盖窗口的留黑区域。它们全部属于当前活动场景：退出场景（包括缓存复用）、故障恢复、关闭或内容清理时移除，句柄失效；不支持跨场景保留。

```cpp
#include "engine/effects/effect_service.h"

// 闪白：立即显示，然后在 0.2 秒内淡出。
std::optional<elysia::effects::ScreenEffectHandle> flash_white()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {255, 255, 255};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.2;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// 图片必须已通过项目的纹理资源配置加载。
std::optional<elysia::effects::ScreenEffectHandle> show_screen_image()
{
    elysia::effects::ScreenImageEffectRequest request;
    request.texture_key = "screen.blood"; // 项目提供的纹理 key
    request.fit = elysia::effects::ScreenEffectFit::Cover;
    request.playback.fade_in_seconds = 0.1;
    request.playback.hold_seconds = 0.3;
    request.playback.fade_out_seconds = 0.4;
    request.playback.layer = elysia::effects::ScreenEffectLayer::BeforeUi;
    return ELYSIA_EFFECTS->request_screen_image_effect(request);
}
```

纯色请求与图片请求共享 `ScreenEffectPlayback`。默认目标透明度为 1，淡入/淡出为 0 秒，保持 0.6 秒，结束策略为 `Timed`，层级为 `AfterUi`，时钟为 `Unscaled`。透明度从 0 线性淡入到目标值，保持结束后线性淡出；淡入为 0 时创建即使用目标透明度。颜色 alpha、图片透明通道与播放透明度相乘。

`Timed` 的保持时间从淡入完成后开始；`Manual` 淡入后持续保持，直到主动结束。`stop_screen_effect(handle)` 从当前透明度开始，使用请求中的淡出时长；为 0 时立即移除。已经淡出的特效重复 stop 返回 false，不重启淡出。`cancel_screen_effect(handle)` 立即移除。`is_screen_effect_active(handle)` 查询是否存在，不代表淡入是否完成。失效句柄安全返回 false，不能控制复用槽位的新特效。

`Unscaled` 使用原始帧时间，不受时间缩放和场景暂停影响。`Scene` 使用缩放后时间，并跟随场景暂停。每帧先推进已有屏幕特效，再更新场景；在本帧输入、场景进入或更新中创建的特效从下一帧开始计时。请求不调用用户回调，可在场景对象更新中创建或取消。

`BeforeUi` 位于世界和调试图形之后、UI 之前；`AfterUi` 覆盖 UI。开发工具面板在两者之后绘制。同一层内按创建顺序叠加。图片默认 `Stretch` 拉伸铺满；`Cover` 等比居中铺满并裁切；`Contain` 等比居中完整显示，未覆盖区域保持透明。

创建返回 optional 句柄。没有活动场景、纹理缺失、时间为负或非有限、透明度不在 0–1、定时请求的三个时长全为 0 时返回空值并记录日志。图片借用已加载纹理，资源重载前必须通过现有内容清理流程清除特效；直接释放仍被引用的纹理不受支持。不支持 shader 后处理。

### 屏幕动画与共享布局

`request_screen_animation_effect` 使用与世界动画特效相同的 `effect_key`，共享已加载的动画、`Atlas` 和纹理，每次请求有独立播放实例。默认 `loop=false`、`fit=Contain`、`end=AnimationFinished`、保持时长 0。资源定义中的循环设置不覆盖屏幕请求；角度默认沿用特效定义，也可以通过 `angle_degrees` 覆盖，`flip` 默认 None。

下面是活动场景函数中的局部片段，需包含 `effect_service.h`。各次请求独立，实际调用后都应检查返回的 optional 句柄；片段中的图片键需由项目预先加载。

```cpp
// 单次居中演出：完整显示动画，播放完后淡出。
elysia::effects::ScreenAnimationEffectRequest single;
single.effect_key = "effect.test";
single.playback.fade_in_seconds = 0.2;
single.playback.fade_out_seconds = 0.4;
const std::optional<elysia::effects::ScreenEffectHandle> single_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(single);

// 循环全屏遮罩：等待调用方 stop/cancel。
elysia::effects::ScreenAnimationEffectRequest overlay = single;
overlay.loop = true;
overlay.fit = elysia::effects::ScreenEffectFit::Cover;
overlay.playback.end = elysia::effects::ScreenEffectEnd::Manual;
const std::optional<elysia::effects::ScreenEffectHandle> overlay_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(overlay);

// 右下角动画：原始尺寸，向内偏移 24 个逻辑像素。
elysia::effects::ScreenAnimationEffectRequest corner = single;
corner.fit = elysia::effects::ScreenEffectFit::Natural;
corner.placement.anchor = elysia::effects::EffectAnchor::BottomRight;
corner.placement.offset = {-24, -24};
corner.placement.scale = {0.5f, 0.5f};
const std::optional<elysia::effects::ScreenEffectHandle> corner_handle =
    ELYSIA_EFFECTS->request_screen_animation_effect(corner);

// 静态图片也使用相同布局参数。
elysia::effects::ScreenImageEffectRequest image;
image.texture_key = "demo.screen_effect";
image.fit = corner.fit;
image.placement = corner.placement;
const std::optional<elysia::effects::ScreenEffectHandle> image_handle =
    ELYSIA_EFFECTS->request_screen_image_effect(image);
```

动画与淡入从下一帧同步推进。`AnimationFinished` 等动画和淡入均完成后淡出，忽略保持时长；循环动画不能使用此策略。非循环动画在 `Timed`/`Manual` 中完成后保持最后一帧；stop 淡出期间动画继续推进。每帧持续一个完整帧间隔，单次时长为帧数 / FPS，大 delta 的剩余时间会推进淡出。

图片和动画共享 `ScreenEffectPlacement`：默认居中、偏移 `{0,0}`、缩放 `{1,1}`。先计算适配尺寸、应用缩放，再将内容锚点与视口同名锚点对齐并添加偏移；九宫格锚点控制摆放及 Cover 裁切方向。全部模式裁切到逻辑视口。`Natural` 使用图片原始尺寸，动画优先使用特效默认尺寸，否则使用首帧尺寸；其余模式按首帧宽高比适配。动画尺寸不随帧变化。旋转中心是目标矩形中心，与定位锚点独立。

偏移和角度必须有限，缩放必须有限且为正。缺失特效、动画或有效帧资源，以及无效结束策略组合，都会拒绝创建。纯色和图片不接受 `AnimationFinished`。

### 两个场景分别完成黑幕转场

旧场景请求渐黑，并用与特效相同的时钟累计时间；达到淡入时长后按现有场景路由接口请求切换。使用 `Manual` 让黑幕在切换前持续保持。创建失败时不要启动转场计时。

```cpp
std::optional<elysia::effects::ScreenEffectHandle> begin_exit_blackout()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.fade_in_seconds = 0.4;
    request.playback.end = elysia::effects::ScreenEffectEnd::Manual;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}

// 在新场景 on_enter 中调用，在首次绘制前创建全黑遮罩。
std::optional<elysia::effects::ScreenEffectHandle> begin_enter_blackout()
{
    elysia::effects::ScreenColorEffectRequest request;
    request.color = {0, 0, 0};
    request.playback.hold_seconds = 0.0;
    request.playback.fade_out_seconds = 0.4;
    return ELYSIA_EFFECTS->request_screen_color_effect(request);
}
```

默认时钟下，旧场景从下一次更新开始累计 `ELYSIA_TIME->raw_delta()`，累计到 0.4 秒时发出切换请求。退出旧场景会清理旧遮罩；新场景进入时创建的新遮罩从全黑开始淡出。因此新场景第一帧仍为黑色，不需要跨场景句柄。场景路由和转场计时由游戏负责。

### 在游戏中手动测试

主菜单 → 模块展示 → 特效，标题说明下方的屏幕特效操作区支持鼠标按钮和快捷键；底部保留飘字操作区：

| 按键 | 效果 |
|---|---|
| F1 | 闪白并自动淡出 |
| F2 / F3 | 渐黑后自动恢复 / 渐黑后持续保持 |
| F4 / F5 | 从当前透明度淡出 / 立即取消 |
| F6 / F7 / F8 | 图片 Stretch / Cover / Contain |
| F9 | 切换 UI 前后层级，清除当前预览后重新选择效果 |

面板显示播放中、已完成或请求失败状态，文案支持项目五种语言。每次选择效果会替换上一次预览。黑屏遮住按钮时仍可用快捷键；Esc 和返回按钮优先清除正在播放的效果，没有活动效果时才返回调用方。图片使用已加载的 `demo.screen_effect` 测试纹理；其 3:2 网格、四角标记和圆环用于观察拉伸、裁切、等比显示和透明叠加。

动画控制区提供播放、循环、结束策略、适配、锚点、偏移和缩放选项，以及按当前布局播放图片的操作。参数变更用于下一次播放；F4/F5 同样控制动画。开启循环会将动画完成策略切换为 Manual，选择动画完成策略会关闭循环。动画使用已加载的 `effect.test`，同时创建同资源键的世界特效作为对照。

这些操作用于人工观察，不是自动化测试结果。启用开发覆盖层时，F2 也用于覆盖层开关；发生快捷键占用时使用页面按钮。实现见[特效展示场景](../../../../game/showcase/effects/effects_showcase_scene.cpp)。

## 资源配置

先准备图片及动画声明，再让特效定义引用已经声明的动画。注册表的 `required.animations` 与 `required.effects` 分别指向这两份清单。

### 添加核心动画

将横向排列的 14 帧图片放到 `assets/textures/test/frame_group.png`，在动画清单中声明：

```json
{
  "animations": [
    {
      "key": "test.animation",
      "path": "test/frame_group.png",
      "frame_count": 14,
      "fps": 10,
      "loop": false,
      "horizontal_strip": true
    }
  ]
}
```

图片应只有一行等宽帧，宽度能被帧数整除。加载成功后，用 `test.animation` 创建本页前文介绍的动画实例，并由所属对象推进更新。使用逐帧目录或实体动画时，按[动画格式参考](../../reference/resources/animation-and-effects.md)提供帧前缀、布局和配置。

### 把动画声明为特效

在特效清单中把动画资源键映射为特效资源键：

```json
{
  "effects": [
    {
      "key": "effect.test",
      "animation_key": "test.animation",
      "default_width": 128,
      "default_height": 128,
      "default_angle_degrees": 0
    }
  ]
}
```

内容加载成功后，通过特效服务请求 `effect.test`。特效定义不会代替动画加载；引用不存在的动画会失败。模块内特效、分段编号和尺寸约束见[特效格式](../../reference/resources/animation-and-effects.md#特效)。

配置错误时，先检查注册表是否引用了正确清单，再检查图片尺寸、帧文件是否齐全、动画键是否存在，以及同类型资源是否重复。文件系统分段编号与运行时资源键不同，完整规则集中在[帧来源与分段](../../reference/resources/animation-and-effects.md#帧来源与分段)。

## 常见误用与参考

动画不会因为创建就自动更新；由所属 `Updatable` 每帧驱动一次。特效已经是场景对象，不再手动更新。不要在绘制时推进动画，不在资源失效后继续使用旧 `Atlas`，也不要用动画完成回调决定必须发生的游戏规则结果。

- [动画接口](../../../../engine/animation/animation.h)、[特效请求参数](../../../../engine/effects/effect_types.h)
- [场景生命周期](../scene/scene.md)、[时间](time-and-timers.md)、[返回使用指南](../README.md)
