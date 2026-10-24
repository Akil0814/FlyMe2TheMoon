# 输入与控制器接口迁移

本文面向迁移旧输入代码的项目维护者，列出已经实施的接口变化及替代入口。迁移时应同时检查生命周期和返回结果，不只替换类型名称。

当前接口不保留兼容别名或旧入口转发；仓库内示例和测试已迁移。

| 旧接口／行为 | 新接口／行为 |
| --- | --- |
| DevelopmentInputCapture 与旧头文件 | `InputCapture`，包含 engine/input/input_capture.h；位检查使用 captures_input |
| `InputSystem`::current_device、InputDeviceTracker | 移除；采集按事件分类，UI 自行维护活动设备 |
| `InputSystem`::end_frame、`GamepadInputTranslator`::reset | 移除空生命周期或无用入口 |
| 原始／UI 滚轮整数 | float，消费者保留小数，整数来源显式转换或使用浮点字段 |
| bind_target、unbind_target、replace_input_map 的布尔成功判断 | `ControllerOperation`；明确检查 Pending、Succeeded、Failed |
| 设备绑定／解绑／转移／替换的 bool | expected<void, `InputBindingError`>；检查或记录 error |
| `ControllerService`::remove 的 bool | expected<void, `ControllerError`> |
| 捕获过滤隐式推进状态 | observe 更新门控，const filter 只读取 |
| `LocalPlayerRegistry`::reset | 删除重复入口，使用 reset_defaults |
| `RawInputState`::clear_keyboard / clear_pointer / clear_gamepad | 删除未使用的分类清零入口；物理源独立，清空一个源使用 clear，单个控制使用 clear_control |
| 游戏修改取消处理器或键盘权限维护状态 | 内部接口不再对游戏开放，使用场景的路由配置和 `ControllerService` 的映射替换 |
| `engine/input/scene_input_router.h` 和 `elysia::input::SceneInputRouter` | `engine/scene/input/scene_input_router.h` 和 `elysia::scene::SceneInputRouter` |
| `elysia::input::UiInteractionMode` / `UiDeviceAccess` | `elysia::scene::UiInteractionMode` / `UiDeviceAccess`，随场景路由归属迁移 |
| `game/input/local_controls.h` / `command_view.h` 和对应 `example::input` 控制入口 | `game/gameplay/control/` 下同名头文件，使用 `example::gameplay::configure_scene_player`、`session_player`、`existing_session_player` 和 `CommandView` |
| 直接调用 `session_player_handles()` 修改会话句柄表 | 移除公开存储入口；通过 `session_player` 创建/查询，或 `existing_session_player` 只查询 |

配置调用在回调内可能排队，不能用 `!succeeded()` 直接认定失败；需要等待的调用方保存结果，在更新阶段处理终态。仅允许同步执行的初始化流程应要求 Succeeded，并将 Pending 视为调用时机不满足要求。多目标相机示例保存换绑请求，仅在成功后改变主目标。

取消旧目标前的绑定校验失败保留旧绑定；取消回调执行后才发现目标失效时保持解绑。取消不是按键释放，不应触发松键攻击。每次失焦只通知一次 FocusLost，暂停只通知 Paused，持续不可用不重复通知。

配置提交的回调异常会将操作置为 `Failed / CallbackFailed`，清理受影响绑定和同一控制器的待提交请求，再向场景异常边界传播。不能把 `CallbackFailed` 当成可忽略的普通映射校验失败。结束会话先让全部句柄失效，再完成每个控制器和目标的取消通知；通知失败仍完成其他清理。取消回调内 `begin_session()` 返回 `SessionEnding`，可在结束调用的异常处理完成后重新开始。

`set_shortcut_devices` 同时限制 on_shortcuts 的帧状态与事件。批量配置引入的键盘分区 ID 不会被随后 create_partition 覆盖；分区 ID 分配耗尽返回 `PartitionIdsExhausted`。

相关自动化测试使用合成输入。实体手柄拔插、触控板高精度滚动及开发覆盖层实机体验仍应执行硬件验收，不以合成测试替代。
