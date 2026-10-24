# 游戏对象与绘制

本文说明如何为世界对象接入更新和绘制，以及如何在场景中管理它。创建对象、让对象每帧运行和让对象可见是三个不同步骤，下面逐项解释。

`GameObject` 表示世界中的对象。它提供位置、尺寸、状态和绘制扩展点，不自动加载图片、响应输入或参与物理。需要持续行为时实现 `Updatable`，需要碰撞时接入[物理](../systems/physics.md)，角色输入通过[控制器](../gameplay/control.md)。UI 使用独立的 [UiElement 体系](../ui/README.md#create-window)。

## 创建一个会移动的可见对象

将下列类型放在游戏头文件中；在场景 `on_enter()` 中调用 `create_and_add_object<MovingBlock>()` 并检查返回值。重复进入时保留已有实例或明确清理，不能反复创建。

所属场景必须在 `SceneRuntimeFeatures` 或 `GameplaySceneFeatures` 中配置 `.camera = elysia::scene::CameraSceneConfig{}`，并把镜头中心设置到对象附近。默认场景没有世界相机；仅加入会提交世界绘制命令的对象会触发渲染契约错误。完整场景接入见[相机示例](../systems/camera.md#配置跟随)。

```cpp
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/colors.h"

class MovingBlock final : public elysia::core::GameObject,
                          public elysia::core::Updatable
{
public:
    MovingBlock() : GameObject(elysia::core::DepthLayer::Item)
    {
        set_world_rect({40.0f, 40.0f, 32.0f, 32.0f});
    }

    void update(double delta) override
    {
        // delta 已包含全局倍率；这里只额外应用该对象自己的倍率。
        elysia::core::Vector2 next_position = position();
        next_position.x += 50.0f * static_cast<float>(scaled_delta(delta));
        set_position(next_position);
    }

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& commands) const override
    {
        elysia::core::RenderCommand command;
        command.type = elysia::core::RenderCommandType::FillRect;
        // 保留世界坐标，统一投影由场景在提交后完成。
        command.command_rect = render_rect();
        command.color = elysia::core::colors::white;
        commands.push_back(command);
    }
};
```

对象加入场景时才注册更新接口；不要又从场景手动调用它的 `update()`。对象自身时间缩放需要显式使用 `scaled_delta()`，引擎不会自动替你乘一次。该示例没有物理刚体，直接改位置即可。

`add_object` 同步注册；不要在对象 `update()`、UI 呈现动画、渲染、查询或物理模拟步回调中增删场景对象。在对象中记录生成请求，由场景 `on_before_update` / `on_after_update` 处理。场景固定步钩子和输入快照回调也允许同步添加；注册回调允许嵌套添加，但不能清空场景。完整规则见[阶段表](scene.md#常用操作与扩展点)。

## 位置、绘制与资源

`position()` 是世界矩形左上角，`center()` 是中心；分别使用 `set_position`、`set_center`、`set_size`、`set_world_rect` 修改。逻辑使用 `world_rect()`，绘制使用 `render_rect()`，后者可能包含物理插值偏移。已有物理刚体的瞬移通过物理接口完成。

纹理绘制仍在 `submit_render_commands()` 中填充命令：`type = Texture`、`texture = 已加载纹理`、`command_rect = render_rect()`。裁切设置 `use_src_rect = true` 和纹理坐标中的 `src_rect`；`alpha` 为 0–255，`rotation_degrees` 是顺时针角度，`rotation_origin` 为矩形内归一化旋转中心，`flip` 使用 `SpriteFlip`。纹理为空时不提交该条命令。

通过 [ResourceService](../content/resources.md) 查询纹理并检查空指针；返回资源是借用，不能自行释放。资源重载前必须停止使用旧纹理。动画对象通过[动画接口](../systems/animation-and-effects.md)生成同类命令。不要在绘制函数中加载资源、推进计时或修改游戏规则。

世界命令由场景使用当前相机统一投影，不要先手动转为屏幕坐标。世界图元还支持矩形边框、圆、线和三角形；调试可视化优先使用[调试绘制](../tools/debug-draw.md)。UI 绘制使用独立命令。

## 状态与排序

| 操作 | 使用契约 |
| --- | --- |
| `set_visible(false)` | 停止该对象绘制，不等于停止更新 |
| `set_active(false)` | 不参与普通对象更新；不能把它等同于完整注销物理或停止外部服务 |
| `set_update_when_paused(true)` | 允许普通更新在场景暂停时继续；不会恢复整个物理世界步进 |
| `set_receive_input_when_paused(true)` | 输入接收策略的一部分，不会自动给对象增加输入接口 |
| `destroy()` | 标记销毁，停止继续使用其引用；实际释放由所属流程完成 |
| `reset()` | 恢复基础状态；`GameObject` 还恢复时间缩放与显示偏移，业务字段由派生类负责 |

绘制层从后向前依次为 Background、Terrain、EffectBack、Item、Character、EffectFront、Foreground；同层较大的构造参数 `order` 后绘制。层与顺序在构造时指定，`Count` 是边界标记。UI 在世界之后绘制。排序不代表输入优先级或物理碰撞层。

`GameObject::reset()` 不会替你恢复出生位置、生命值、订阅关系或动画状态。派生类重写时调用基类，再恢复自己的字段。场景持有对象；成员中保存的其他对象指针只是借用，需要在目标移除时清空，不能在对象销毁后用裸指针检查“它还活着吗”。

对象的 `reset()` 与场景的 `on_reset()` 是不同接口。场景重置不会自动重置所有对象；业务按需逐个恢复或清空重建。场景在更新末尾、退出回调后和重置回调后回收已标记对象；`on_scene_object_removing`（`GameplayScene` 使用 `on_control_target_removing`）执行时对象仍有效，应在此解除借用引用。

## 参考

- [场景与对象管理](scene.md)、[核心概念](../core_concepts.md)
- [绘制命令定义](../../../../engine/core/render/render_command.h)、[GameObject](../../../../engine/core/game_object.h)
- [返回使用指南](../README.md)
