# 配置读取与用户设置

本文区分启动默认值、玩家设置和只读游戏配置。根据数据用途选择服务，再处理加载、应用或保存结果，避免将界面草稿视为已经生效的配置。

| 配置 | 谁提供、何时使用 |
| --- | --- |
| `AppConfig` | 项目启动默认值，启动前编辑 JSON |
| `UserConfig` | 玩家设置，通过 `ELYSIA_USER_CONFIG` 应用与保存 |
| 游戏配置 | 内容加载后通过 `ELYSIA_CONFIG` 只读查询 |

应用负责初始化用户设置及运行时回调，内容加载流程发布游戏配置；游戏对象只使用读取、应用与保存接口，不自行初始化、发布快照或关闭服务。加载与文件入口见[资源](resources.md)。

## 读取游戏配置

在 `assets/configs/manifests/config_manifest.json` 的 configs 中加入 `"game": "configs/game/rules.json"`，并创建对应文件，例如 `{"difficulty":{"enemy_health_scale":1.0}}`。以下函数在内容加载完成后调用：

```cpp
#include "engine/config/config_service.h"
#include "engine/tools/logger.h"

#include <optional>
#include <expected>

std::optional<double> enemy_health_scale()
{
    const std::expected<double, elysia::config::ConfigAccessFailure> result =
        ELYSIA_CONFIG->get_double("game.difficulty.enemy_health_scale");
    if (!result)
    {
        ELYSIA_LOG_ERROR("config", result.error().message);
        return std::nullopt;
    }
    // 类型检查属于配置服务；生命倍率必须为正是游戏自己的业务约束。
    if (*result <= 0.0)
        return std::nullopt; // 游戏自己的业务约束。
    return *result;
}
```

调用方遇到空结果时停止依赖该配置的生成操作或采用项目明确规定的默认值。服务不会自动回退。`contains(key)` 只说明存在，不保证目标 getter 的类型兼容。

公开读取接口为 `get_int`、`get_double`、`get_bool`、`get_string`、`get_vector2`、`get_rect` 及对应的 `_array` 版本。失败包括 `NotInitialized`、`MissingKey`、`TypeMismatch`、`InvalidValue`；返回值拥有自身数据，不是对配置 JSON 的借用。

get_int 要求 int64 范围内的整数；get_double 接受有限数值，包括整数。布尔和字符串不作隐式转换。`Vector2` 必须是恰好包含 x、y 的对象；`Rect` 必须恰好包含 x、y、width、height，尺寸非负，分量须能表示为有限 float。数组 getter 对每项应用相同规则，不混入其他类型或 null。

## 修改并保存玩家设置

以下辅助函数供设置页面的 Save 操作调用。需要草稿编辑时，在进入页面时保存 `runtime_state()` 作为回滚基线，编辑独立的 `snapshot()` 副本，保存时调用带基线的重载。

```cpp
#include "engine/config/user_config_service.h"
#include "engine/tools/logger.h"

bool save_settings(const elysia::config::UserConfigData& draft,
                   const elysia::config::UserConfigRuntimeState& baseline)
{
    // baseline 是进入编辑页面时保存的运行状态，失败时据此尝试回滚。
    const std::expected<elysia::config::UserConfigApplyStatus,
        elysia::config::UserConfigCommitFailure> result =
        ELYSIA_USER_CONFIG->apply_and_save_user_config(draft, baseline);
    if (!result)
    {
        const elysia::core::FailureDiagnostic diagnostic =
            elysia::config::to_failure_diagnostic(result.error());
        elysia::tools::Logger::instance()->log_stream(
            elysia::tools::LogLevel::Error, "settings",
            [&](std::ostream& output)
            {
                output << elysia::core::format_failure_diagnostic(
                    diagnostic, "SETTINGS", "settings");
            }, diagnostic.origin);
        return false; // 页面重新读取实际状态，不假设回滚必然成功。
    }
    if (*result == elysia::config::UserConfigApplyStatus::PendingRestart)
        ELYSIA_LOG_INFO("settings", "Restart required for saved settings.");
    return true;
}
```

单项即时应用可通过 `user_config().set_master_volume`、set_music_volume、set_sound_volume、set_language、set_target_fps、set_window_settings、set_vsync；检查 expected 后再提示成功。单项 setter 不等于保存磁盘，`save_user_config()` 负责持久化当前设置；保存失败时不能宣称已经保存。

`snapshot()` 返回设置副本，`runtime_state()` 还包含重启状态，`is_dirty()` 查询未保存变化，`restart_required()` 查询待重启要求。批量事务的无基线重载使用调用时的状态作回滚基线。更完整的页面流程见[内置设置场景](../builtin-scenes/settings.md)。
## AppConfig 与 UserConfig

`assets/configs/global/app_config.json` 使用严格的第 2 版结构约定，提供窗口标题和窗口、渲染、音频、本地化的默认值。逻辑分辨率不属于用户配置，只由 `ApplicationDescriptor` 提供：

```json
{
  "schema_version": 2,
  "window": {
    "title": "Elysia Engine",
    "mode": "windowed",
    "windowed_size": { "width": 1280, "height": 720 }
  },
  "render": { "fps": 60, "vsync": true },
  "audio": { "master_volume": 100, "music_volume": 100, "sound_volume": 100 },
  "localization": { "language": "en" }
}
```

未知、缺失、重复或非法字段会使启动失败。窗口宽高和 FPS 必须为正，音量范围为 `0..100`，标题和语言不能为空。`render.fps` 是最大呈现帧率；VSync 等待和本帧更新、渲染耗时都会计入同一帧预算，只有剩余预算会由软件限帧器等待。

`player_data/user_config.json` 保存完整的 window、render、audio 与 localization 快照，但不保存窗口标题。`AppConfig` 和 `UserConfig` 都只接受严格 v2；旧 v0/v1 `UserConfig` 会作为无效配置归档，并以 `AppConfig` 默认值重建。`.tmp`/`.bak` 恢复、损坏主文件归档和未来版本保护继续保留。

窗口模式只接受 `windowed` 和 `borderless_fullscreen`。`windowed_size` 始终记录窗口模式使用的大小；进入无边框桌面全屏不会覆盖它。

内建 `SettingsScene` 使用草稿式提交：控件编辑不会立即修改运行时；Save 通过`UserConfigService::apply_and_save_user_config()` 批量应用并持久化。事务显式携带进入页面（或上次保存成功）时的`UserConfigRuntimeState` 作为回滚基线，因此应用失败或持久化失败不会回滚到点击 Save 前偶然变化的状态。回滚失败会作为独立错误返回，页面随后以 `UserConfig` 的实际运行时状态刷新。

FPS 上限会立即应用到软件限帧器，VSync 会保存为下一次启动使用的值并显示重启提示。如果 VSync 又切回本次启动实际采用的值，待重启状态会自动取消。内置面板在同一滚动区域中按显示、音频和通用分组，标题、状态消息及 Save/Back 操作保持固定；界面入口见[设置场景](../builtin-scenes/settings.md)。

## 通用游戏玩法配置

入口是 `manifests.required.configs` 指向的 `assets/configs/manifests/config_manifest.json`：

```json
{
  "schema_version": 1,
  "configs": {}
}
```

`configs` 是命名空间到文档路径的映射。上例为空配置；接入角色、技能或关卡配置时，在此添加命名空间和路径，由内容加载阶段统一读取。资源清单自身是严格结构约定；被引用的文档根节点可为 object、array、string、boolean 或 number，但不能为 `null`。

根值使用命名空间配置键注册；对象字段递归展开；数组下标使用无补位 `.0/.1/...` 组成部分。例如：

```json
{
  "difficulty": { "enemy_health_scale": 1.0 },
  "spawn_points": [{ "x": 100, "y": 200 }]
}
```

若命名空间为 `game`，可访问 `game.difficulty.enemy_health_scale` 与 `game.spawn_points.0.x`。命名空间和对象字段组成部分必须符合 `[A-Za-z0-9_]+`。重复 JSON 属性、任意层级的 `null`、非法组成部分和完整配置键冲突都会在快照发布前失败，并保留配置路径、JSON 路径指针与首次与重复声明来源。

游戏代码通过 `ConfigService` 的 `contains`、标量/几何 getter 及数组 getter 读取配置。`publish` 与关闭操作由引擎加载流程负责。访问失败会返回 `expected` 并按错误类型去重记录日志；不暴露原始 JSON，也不提供 optional、隐式回退值、热重载或业务专用接口。

## 常见误用与参考

不要把音频服务即时音量、本地化服务即时语言修改等同于保存用户偏好；需要持久化时走用户配置接口。不要用只读游戏配置保存进度，使用[存档](save.md)。修改磁盘 JSON 不会自动热重载；重新加载内容可能使资源引用失效，见[资源生命周期](resources.md)。

- [配置接口](../../../../engine/config/config_service.h)、[用户设置接口](../../../../engine/config/user_config_service.h)
- [可选架构说明](../../architecture/subsystems/runtime-config.md)、[返回使用指南](../README.md)
