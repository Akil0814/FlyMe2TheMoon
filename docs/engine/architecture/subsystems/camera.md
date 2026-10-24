# 相机架构与运行流程

本文解释相机如何在全局槽位与场景运行时之间分工，以及跟随、手动运动和最终呈现如何协调。游戏接入示例见[相机使用指南](../../user-guide/systems/camera.md)。

## 模块结构

相机模块由三个层次组成：

```text
CameraManager（全局唯一拥有者和写入口）
  ├─ Main       Camera + CameraController
  ├─ Cinematic  Camera + CameraController
  ├─ Auxiliary1 Camera + CameraController
  └─ Auxiliary2 Camera + CameraController

SceneCameraRuntime（可选的场景能力）
  └─ owned slots、最终呈现槽、混合状态和场景句柄所有权
```

- `Camera` 保存最终渲染中心、屏幕视口大小和缩放倍率，并负责世界坐标与屏幕坐标之间的转换。
- `CameraController` 保存逻辑中心、焦点、世界边界、跟随策略、震屏和单槽姿态运动。
- `CameraManager` 固定拥有四组相机，不提供动态创建或销毁接口。
- `SceneCameraRuntime` 保存最终呈现槽和可选的槽间混合状态，并限制场景只能操作声明拥有的槽和句柄。
- `Scene` 只协调焦点解析、生命周期和完成事件派发，不保存相机播放状态。

四个槽位定义如下：

| 槽位 | 用途 |
| --- | --- |
| `Main` | 场景默认使用的世界相机。 |
| `Cinematic` | 过场和演出视角。 |
| `Auxiliary1` | 无预设用途的扩展相机。 |
| `Auxiliary2` | 无预设用途的扩展相机。 |

## 访问规则

`CameraManager` 是底层相机服务。相机子系统可以直接使用它；场景代码应通过 `camera_runtime()` 操作本场景声明拥有的槽。外部只能取得 `const Camera&`，用于渲染、查询可见区域和坐标投影：

```cpp
const auto& camera = elysia::camera::CameraManager::instance()->camera(
    elysia::camera::CameraSlot::Main
);
```

相机状态必须通过 Manager 修改：

```cpp
auto* cameras = elysia::camera::CameraManager::instance();

cameras->set_viewport_size(CameraSlot::Main, { 1280.0f, 720.0f });
cameras->set_zoom(CameraSlot::Main, 2.0f);
cameras->set_world_bounds(CameraSlot::Main, world_bounds);
cameras->set_follow_strategy(
    CameraSlot::Main,
    std::make_unique<SmoothFollowStrategy>(300.0)
);
```

配置接口立即生效，包括中心、视口、缩放、焦点、边界和跟随策略。直接设置中心或缩放会取消该槽正在播放的姿态运动，并同步 `Controller` 与最终 `Camera`，避免两个控制源同时写入。

## 视口、缩放与坐标

`viewport_size` 使用逻辑渲染坐标中的屏幕单位，`zoom` 默认为 `1.0`，合法范围为 `[0.1, 10.0]`。普通越界值会被钳制，非有限值回退为 `1.0`。相机在世界中实际可见的尺寸为：

```text
world_viewport_size = viewport_size / zoom
```

`world_to_screen` 和 `screen_to_world` 同时支持点和矩形。矩形转换会同时变换位置与尺寸；屏幕坐标是相机视口局部坐标，不含窗口位置或 UI 布局偏移。UI 命令不经过这些转换。

## 请求与更新时序

震屏、立即对焦和清除效果是 FIFO 瞬时请求；中心与缩放动画使用统一的姿态运动：

```cpp
cameras->request_shake(CameraSlot::Main, shake_params);
cameras->request_snap_to_focus(CameraSlot::Cinematic);
cameras->request_clear_effects(CameraSlot::Main);

const auto motion = cameras->move_camera_to(
    CameraSlot::Main, {.center = destination, .zoom = 2.0f}, 0.5);
```

请求按提交顺序进入单线程 FIFO 队列。`CameraManager::update(slots, delta)` 的执行顺序为：

1. 按 FIFO 顺序处理所有待执行请求。
2. 更新调用方指定的 `CameraSlotSet`。
3. `Controller` 推进活动姿态运动；播放期间暂时取得中心和缩放控制权。
4. 没有姿态运动时，`Controller` 应用跟随策略返回的中心与可选 zoom。
5. 按当前倍率限制世界边界，再叠加震屏偏移。
6. 最终中心写回 `Camera`，并返回本帧自然完成的 motion 值事件。

同一槽保存一个姿态运动和一个震屏，两者可以并行。路径节点使用从上一姿态到目标姿态的正时长和统一缓动；新运动从当前逻辑姿态连续替换旧运动。清除效果停止震屏但不取消姿态运动。直接设置姿态、吸附焦点或显式取消会结束运动，且不会产生自然完成事件。

DeadZone 使用相机视口局部屏幕像素定义。焦点会先按当前 zoom 投影后再与死区比较，因此改变倍率不会改变死区在屏幕上的视觉大小。

## 场景集成

场景不拥有 `Camera` 或 `CameraController`。相机是可选运行时能力，基础场景行为为：

- 场景用可选的 `CameraSceneConfig` 声明相机运行时；默认 `Scene` 和 `GameplayScene` 都没有相机。所有 owned 槽位都会推进；`ResolveEachFrame` 额外调用 `resolve_camera_focus(slot)`。
- 有相机的场景使用最终呈现相机投影世界命令。无相机场景可以正常渲染 UI，但提交世界命令或可见的世界 `DebugDraw` 属于配置错误。
- UI 命令仍直接使用屏幕坐标执行，不经过世界相机。
- `Scene::camera()` 返回最终呈现相机；活动混合期间返回场景的组合相机，其余时间返回已提交槽。

```cpp
std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main || !_player)
        return std::nullopt;
    const auto rect = _player->render_rect();
    return elysia::camera::CameraFocus{rect, rect};
}
```

场景若需要玩家与演出相机并行更新，可以配置：

```cpp
MyScene::MyScene() : Scene(elysia::scene::SceneRuntimeFeatures{
    .camera = elysia::scene::CameraSceneConfig{
        .initial_slot = elysia::camera::CameraSlot::Main,
        .owned_slots = elysia::camera::CameraSlot::Main
                     | elysia::camera::CameraSlot::Cinematic,
        .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}}) {}
```

`camera_runtime().cut_to()` 立即提交槽；`camera_runtime().blend_to()` 捕获当前实际呈现构图作为固定源，并逐帧插值到实时目标槽。中心和缩放参与插值，视口来自目标槽。新混合从当前组合相机继续。场景暂停默认冻结控制器与混合，配置可允许继续推进。

## 场景切换与重置

切换到不同场景，或以 `SceneReloadMode::Reset` / `Recreate` 重进当前场景时，`SceneManager` 会在旧场景 `on_exit()` 之后、新场景 `on_enter()` 之前重置目标场景的全部 owned 槽位。

槽位重置会：

- 清除焦点和世界边界；
- 清除跟随策略、活动运动和效果；
- 清除面向该槽位的未处理请求；
- 将逻辑中心和最终中心归零；
- 将缩放恢复为 `1.0`；
- 保留视口大小。

未声明为 owned 的槽不会因这次切换被破坏。Reuse 当前实例不重置静态姿态，但退出时仍取消活动运动和混合；`SceneManager` 关闭时重置全部槽位。

场景的退出与销毁只取消该 `SceneCameraRuntime` 记录的运动句柄，不按槽位取消另一个场景后来创建的运动。缓存场景的延迟销毁因此不会误停当前场景在同一槽播放的新轨迹。槽位整体重置仍只在目标进入的重置阶段和应用关闭执行。Recreate 先构造候选，再退出当前场景、销毁目标旧缓存并激活候选；构造函数不应修改共享相机，实际配置放在 `on_enter`。

## 当前边界

- 相机请求队列仅用于主线程，不提供线程同步。
- 四个相机的初始中心和视口均为零；应用或场景需要显式设置视口。
- 当前不提供动态相机、分屏视口布局、辅助相机占用仲裁或通用多效果栈。
- 旋转和裁剪属于 `Camera` 与渲染投影层的后续扩展，不应放入跟随策略。
- `InputSystem` 不会自动把指针位置转换到世界坐标；业务需要按所用槽位显式调用 `screen_to_world`。

## 多目标构图

`MultiTargetFollowStrategy` 根据一组矩形平滑调整中心与缩放。它只接收几何值，不持有游戏对象。场景在移动和物理插值之后，从对象的 `render_rect()` 取得呈现矩形，再提供给跟随策略。

以下是派生场景中的局部片段：进入时配置一次策略，焦点回调每帧返回当前目标。`_player` 与 `_companion` 必须指向仍存活的对象。

```cpp
#include "engine/camera/multi_target_follow_strategy.h"

// 在场景进入时配置。
auto& cameras = camera_runtime();
cameras.set_follow_strategy(
    elysia::camera::CameraSlot::Main,
    std::make_unique<elysia::camera::MultiTargetFollowStrategy>());

std::optional<elysia::camera::CameraFocus> MyScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main) return std::nullopt;
    const std::array rects{_player->render_rect(), _companion->render_rect()};
    return elysia::camera::make_camera_focus(rects, 0); // 玩家为主目标。
}
```

同一回调可以为各个拥有的槽位返回不同焦点。`CameraFocus::bounds` 保存目标包围矩形，`primary` 保存选定主目标。列表为空或全部无效时返回 `nullopt`；非有限矩形会被跳过，主目标索引无效时回退到首个有效矩形。支持点和线状目标，直接 `set_focus()` 也会拒绝非有限矩形。

### 配置与缩放决策

| 配置 | 默认值 | 行为 |
| --- | --- | --- |
| `dead_zone_enabled` | `true` | 只移动到足以将目标纳入安全区域的位置；关闭时跟随目标组中心 |
| `safe_ratio` | `0.70` | 居中安全区域占视口宽高的比例 |
| `inner_ratio` | `0.55` | 用于开始放大等待的更小区域 |
| `min_zoom`、`max_zoom` | `0.5`、`2.0` | 自动缩放范围；倍率越小，可见世界越大 |
| `movement_half_life` | `0.12 s` | 固定目标下，中心误差减半所需时间 |
| `zoom_out_half_life` | `0.10 s` | 拉远时较快响应 |
| `zoom_in_half_life` | `0.35 s` | 拉近时较慢响应 |
| `settle_seconds` | `0.4 s` | 拉近或退出仅主目标模式前的等待时间 |

构造时传入 `MultiTargetFollowConfig`，或用 `set_config()` 修改；修改会重置计时与模式。比例限制为 `0 < inner_ratio < safe_ratio <= 1`，缩放范围还受 `Camera` 的全局范围限制。无效的半衰期或等待时间使用默认值。

策略根据目标组宽高计算能放进安全区域的倍率。需要拉远时立即开始平滑调整；拉近则等待目标持续处于内部区域达到规定时间，再向适配安全区域的倍率推进。新的拉远需求会取消拉近过程。移动使用同帧的混合倍率，自动跟随不会重新启动手动缩放动画。

若目标组在 `min_zoom` 下仍放不进画面，相机会趋近该倍率并改为跟随主目标。只有目标组在该倍率下能持续放入内部区域达到等待时间，才恢复整体构图。`primary_only()` 查询此状态。主目标自身大于可见区域时，无法容纳的轴按其中心定位。

### 手动镜头与生命周期

```cpp
const auto motion = cameras.move_to(
    elysia::camera::CameraSlot::Main, {.zoom = 1.5f}, 1.0);
```

活动姿态运动一直控制相机到完成帧；目标中省略的通道保留该段开始时的值。`ResumeFollow` 运动结束后的下一帧恢复跟随和自动缩放。立即 `set_zoom()` 当场生效，下一次更新恢复自动缩放；超出自动范围时平滑返回，不突然跳变。

初次获得和重新获得目标都使用平滑过程。焦点缺失时保留构图并重置计时，显式手动效果仍可推进。替换策略会重置状态，显式吸附请求仍会立即对准目标组边界。原有策略保留首次立即定位行为，不自动改变缩放。

`IFollowStrategy::update()` 是非 const 方法，返回包含中心与可选倍率的 `CameraFollowResult`。自定义策略需从旧 `update_center()` 迁移。没有姿态运动控制相机时，策略可使用 `automatic_zoom_enabled`；`reset()` 清除状态。`snap_on_acquisition()` 默认返回 true，多目标策略重写为 false。

快速分离或传送期间，平滑跟随可能暂时让目标离开屏幕。世界边界优先于构图，震屏在构图后叠加，两者都不保证全部目标始终可见。无效视口、非正或非有限时间步不会推进该策略。

### 演示与验证

从模块展示目录进入相机模块，在跟随页选择多目标策略。WASD 移动主目标，Q/E 横向移动另一对象；操作区提供死区、主目标切换、自动分离与重聚、世界边界、传送、手动缩放和重置。白色轮廓标记主目标，蓝色为安全区域，绿色为内部区域，灰色为目标组边界，红色为可选世界边界。状态区显示倍率与构图模式，无需 ImGui。

运行 `ctest --test-dir out/build/sdl3-Debug -C Debug -L camera --output-on-failure` 验证相机行为。`demo_scene_tests` 覆盖演示控制、返回路由和重入；设置 `ELYSIA_CAMERA_QA_DIR` 可导出 PNG 检查布局。截图检查不代替实际窗口操作。
