# 相机与坐标

本文按场景接入、跟随配置、坐标转换与镜头运动介绍相机。游戏场景通过自己的相机运行时操作声明拥有的槽位，绘制使用最终呈现视角。

场景通过可选的 `CameraSceneConfig` 启用相机，并使用 `camera_runtime()` 配置自己拥有的槽；`Scene::camera()` 返回最终呈现视图。标准场景已负责每帧更新，相机服务不需要游戏重复驱动。

## 配置跟随

以下完整场景类型显式启用相机，在每次进入时配置跟随。视口尺寸由 `SceneManager` 按应用逻辑画布统一同步，场景不单独改写。示例焦点是固定矩形；游戏中可替换为目标对象的 `render_rect()`。

```cpp
#include "engine/scene/scene.h"
#include "engine/camera/follow_strategy.h"

#include <memory>

class FollowScene final : public elysia::scene::Scene
{
public:
    FollowScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
            .camera = elysia::scene::CameraSceneConfig{
                .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}})
    {
    }

protected:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        using namespace elysia::camera;
        // 借用本场景的相机运行时；由场景统一推进，不手动调用更新。
        elysia::scene::SceneCameraRuntime& cameras = camera_runtime();
        cameras.set_zoom(CameraSlot::Main, 1.0f);
        cameras.set_world_bounds(CameraSlot::Main,
            elysia::core::Rect{0, 0, 3000, 2000});
        cameras.set_follow_strategy(CameraSlot::Main,
            std::make_unique<SmoothFollowStrategy>(300.0));
    }

    // 本例没有外部订阅或业务状态；镜头活动由场景生命周期清理。
    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(
        elysia::camera::CameraSlot) const override
    {
        // 单目标示例：包围区域与主目标使用相同矩形，返回的是几何值副本。
        const elysia::core::Rect target{100, 100, 32, 32};
        return elysia::camera::CameraFocus{target, target};
    }
};
```

在场景类中重写 `resolve_camera_focus(CameraSlot)`，将目标的 `render_rect()` 作为 `CameraFocus` 的两个矩形；没有有效目标时返回 `std::nullopt`。构造场景时把所用槽写入 `CameraSceneConfig::owned_slots`，并选择 `CameraFocusMode::ResolveEachFrame`。引擎会分别解析并推进每个 owned 槽位；场景持有的目标指针仍须在对象移除时清空。

## 常用操作

| 任务 | 接口与行为 |
| --- | --- |
| 固定镜头 | `set_center(slot, position)`，配合无跟随策略 `set_follow_strategy(slot, nullptr)` |
| 直接跟随 | `HardFollowStrategy` |
| 匀速追踪 | `SmoothFollowStrategy(speed)`，速度为世界单位/秒 |
| 死区跟随 | `DeadZoneFollowStrategy(rect)`，矩形是视口局部逻辑坐标 |
| 限制地图范围 | `set_world_bounds(slot, rect)`；传 nullopt 移除 |
| 即时缩放 | `set_zoom(slot, value)`，取消该槽位姿态运动 |
| 平滑移动或缩放 | `camera_runtime().move_to(slot, CameraPoseTarget{...}, seconds)` |
| 多节点轨迹 | `camera_runtime().play_path(slot, CameraMotionSpec{...})` |
| 震屏 | `camera_runtime().request_shake(slot, CameraShakeParams{...})`，参数为 amplitude、duration_seconds、frequency_hz |
| 对焦与清除效果 | `camera_runtime().snap_to_focus(slot)`、`clear_effects(slot)` |

setter 直接设置配置，震屏请求在下一次相机更新处理。姿态运动自动从当前逻辑中心和倍率开始，节点时长必须为有限正数；中心和缩放可以单独或同时指定，未指定的通道在该段保持起始值。默认在自然结束后恢复跟随，`CameraMotionEndBehavior::Hold` 会停留在终点。`clear_effects` 只清除震屏等瞬时效果，不取消独立的姿态运动。

## 多目标与相机槽位

包含 `engine/camera/multi_target_follow_strategy.h` 后，可将策略设为 `std::make_unique<elysia::camera::MultiTargetFollowStrategy>()`。重写场景 `resolve_camera_focus(slot)`，按槽把目标矩形组成数组，返回 `elysia::camera::make_camera_focus(rects, primary_index)`。该函数复制几何信息，不持有游戏对象；空列表或全部非法时返回 nullopt。

`MultiTargetFollowConfig` 可设置 safe_ratio、inner_ratio、min_zoom/max_zoom、movement_half_life、zoom_out_half_life、zoom_in_half_life、settle_seconds 和 dead_zone_enabled。安全区域要求 `0 < inner_ratio < safe_ratio <= 1`；配置通过策略构造函数或 `set_config` 传入，后者重置策略计时状态。半衰期越小响应越快，settle_seconds 控制收拢后的放大等待。

多目标策略同时移动和缩放，放大有等待，缩小较快；无法在最小倍率容纳全部目标时优先主目标。姿态运动期间跟随和自动缩放让位，ResumeFollow 运动结束后的下一帧恢复。平滑追踪、地图边界和震屏都可能使目标暂时离开屏幕，不保证每一帧包含所有目标。

固定槽位为 Main、Cinematic、Auxiliary1、Auxiliary2。场景通过 `initial_slot` 和 `owned_slots` 声明初始呈现及占用范围；运行时使用 `cut_to(slot)` 立即切换，或用 `blend_to(slot, spec)` 平滑交接。混合会固定源构图并持续读取目标槽最新状态。多个槽位仍不等于分屏布局。

轨迹与混合都会返回句柄，可查询、暂停、恢复和取消。自然完成通过场景的 `on_camera_motion_completed` 或 `on_camera_blend_completed` 值事件钩子通知；生命周期取消不会伪装成完成。新混合打断旧混合时从当前实际显示构图继续，因此画面不会跳回旧源槽。

## 坐标转换

`world_to_screen(point/rect)` 输出视口局部逻辑渲染坐标；`screen_to_world` 进行反向转换。逻辑可见世界尺寸等于 `viewport_size / zoom`，矩形转换同时改变位置与尺寸。

引擎 `InputSystem` 会先把窗口坐标转为逻辑坐标。用接收到的逻辑指针位置调用当前渲染相机的 `screen_to_world`，才得到世界瞄准位置；相机投影结果不能直接当作实际窗口像素或容器局部坐标。UI 绘制不经过世界相机。

读取对象屏幕显示位置时，对其 `render_rect()` 做投影；游戏判定仍使用 `world_rect()`。不要在提交世界绘制命令前自行投影，否则会重复转换。

## 场景切换与常见误用

切换到不同场景或 Reset／Recreate 当前场景时，只重置目标场景声明的 owned 槽位，保留视口并清除焦点、边界、策略、运动和效果。Reuse 当前实例仍执行退出/进入并取消未完成播放，但保留槽的静态姿态和配置。应用关闭时重置全部槽。

Recreate 先构造候选，再退出当前场景、销毁目标旧缓存，最后激活候选。相机配置应放在 `on_enter`，不要在候选构造时改写共享槽；退出流程取消运动与混合，不产生自然完成事件。`on_after_update` 已位于本帧相机推进之后，在其中修改相机姿态可影响本帧渲染，但新运动到下一次推进才消耗时间。

没有配置相机的场景仍可更新和渲染 UI，但提交世界绘制命令或可见世界 `DebugDraw` 会触发 Render 边界契约错误。手动控制与跟随策略同时启用可能相互影响；选择好该阶段的控制方式。所有请求按主线程使用，不跨线程操作相机队列。

## 相机展示

主菜单 → 模块展示 → 相机（SceneKey 13）提供三个页签：

- **跟随与取景**：切换硬跟随、480 单位/秒平滑跟随、中央 40% 单目标死区、多目标跟随；WASD 移动主角色，Q/E 分离目标。支持主目标切换、自动分离与重聚、远距离瞬移、缩放和边界预设。
- **镜头运动**：移动并拉近或播放三段路径，选择 Linear、SmoothStep、EaseInOutCubic，以及恢复跟随或 Hold。暂停、继续、取消、恢复跟随与震动共用场景操作；Hold 完成后句柄仍可查询和取消。暂停姿态运动会同时冻结该槽位的震动，恢复后继续。
- **演出相机**：直接或平滑切入 Cinematic，也可完整播放远景切入、路径移动、拉近、保持与震动、返回 Main。切入期间角色和自动运动冻结，控制绑定保持；返回完成后恢复自动运动设置。暂停、继续、跳过和重播可用于演出各阶段；混合期间禁用单独震动操作。

F1–F3 切页，R 重置当前页。页签和按钮支持鼠标、焦点导航与确认操作；Esc 和返回按钮直接返回调用方并清理镜头活动。切页初始化新页，进入演出页时保留先前的自动运动选择，直到镜头接管。页面使用既有 `UiTabBar` 和滚动操作行，世界覆盖裁剪到内容区域。

网格和坐标轴用于观察构图变化，黄色节点与连线表示镜头路径，紫色方框为演出地标。蓝色表示多目标安全区，绿色表示死区，红色表示边界，白色表示主目标。状态区显示策略、中心、缩放、运动状态及演出阶段；混合期间显示源槽到目标槽的方向。

## 参考

- [场景](../scene/scene.md)、[输入](input.md)、[场景相机运行时](../../../../engine/scene/runtime/scene_camera_runtime.h)
- [可选架构说明](../../architecture/subsystems/camera.md)、[返回使用指南](../README.md)
