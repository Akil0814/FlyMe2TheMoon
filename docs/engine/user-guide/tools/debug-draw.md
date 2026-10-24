# 调试绘制

本文说明如何显示用于排查问题的区域和图元，以及谁负责清理这些内容。调试绘制只提供可视化，不改变碰撞或游戏规则。

`ELYSIA_DEBUG_DRAW` 收集用于诊断的图元，场景渲染时统一显示。它不创建游戏对象或物理碰撞体，也不替代正式美术渲染。全局启用与类别启用必须同时满足。

## 绘制游戏区域

下面函数可由场景更新调用，前提是场景已显式配置 `CameraSceneConfig`；没有相机时不能绘制这些世界图元，接入见[相机指南](../systems/camera.md)。示例假定本场景统一管理 Gameplay 类别，先清理该类别，再提交本帧全部图元；不要让多个模块各自清理同一类别。

```cpp
#include "engine/tools/debug_draw.h"

void show_interaction_area(const elysia::core::Rect& world_rect)
{
    using elysia::tools::DebugDrawCategory;
    elysia::tools::DebugDraw* draw = ELYSIA_DEBUG_DRAW;
    draw->set_enabled(true);
    // 保留其他系统已启用的类别，只添加本例所需的 Gameplay。
    draw->set_enabled_categories(
        draw->enabled_categories() | DebugDrawCategory::Gameplay);
    // 本例负责该类别的全部图元，每次先清理，再提交当前世界区域。
    draw->clear_categories(DebugDrawCategory::Gameplay);
    draw->draw_rect(DebugDrawCategory::Gameplay, world_rect,
                    elysia::core::Color{0, 255, 0, 255}, 2.0f);
}
```

通常只在开发设置变化时调用启用接口；这里为展示完整前提而放在同一函数中。使用矩形、圆、线、点分别调用 `draw_rect`、`draw_circle`、`draw_line`、`draw_point`。

## 坐标与类别

图元位置、矩形尺寸和圆半径使用世界坐标，经当前场景相机投影。线宽以及点的直径使用屏幕逻辑单位，不随相机缩放。它们不是窗口物理像素大小的保证。

`set_enabled_categories` 和 `clear_categories` 可组合多个类别；单次 `draw_*` 要指定一个有效类别，不能把多个类别的掩码直接当图元类别。未启用、无效类别、非有限坐标、空矩形、非正半径/线宽/点尺寸等无效请求会被忽略，不返回可供判错的布尔值。

## 命令保留与清理

命令不会仅因渲染过一次而自动清空。逐帧追加但不清理会积累旧图元，应按明确所有权使用 `clear_categories()` 或统一 `clear()`。关闭总开关会清空命令；关闭类别会清除该类别已有命令。场景切换流程也会清理调试命令。

物理类别由场景更新根据物理调试快照刷新；游戏自定义图元宜使用 Gameplay 等相应类别，不要依赖手写物理类别图元长期保留。当前场景在世界对象之后、普通 UI 之前绘制调试图元。

Dear ImGui 开发覆盖层可提供调试面板，`DebugDraw` 本身是独立服务，不要求游戏自行调用 ImGui 来绘制这些图元。

## 参考

- [图元与类别定义](../../../../engine/tools/debug_draw.h)
- [开发覆盖层](development-panels.md)、[物理功能](../systems/physics.md)
- [返回使用指南](../README.md)
