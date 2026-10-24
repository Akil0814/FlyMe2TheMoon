# 运行时配置模块

本文说明三类配置的加载与发布流程，并解释应用失败、写盘失败和运行时回滚的区别。修改配置服务时，应先确认所处理的数据属于哪一类。

Elysia Engine 将配置划分为三个职责明确的模块：

| 模块 | 职责 | 加载时机 |
| --- | --- | --- |
| `AppConfig` | 程序启动默认值 | Bootstrap |
| `UserConfigService` | 玩家全局设置、运行时应用与持久化 | Bootstrap，使用 `AppConfig` 默认值 |
| `ConfigService` | 只读游戏玩法配置快照与强类型访问 | `GameContentLoader` 完成内容加载后 |

## 代码目录

`engine/config/` 根目录保留稳定的公开入口及其配对实现：`ConfigService`、`UserConfigService`、`UserConfig`、`user_config_data.h`，以及 `user_config_types.h` 中的错误、状态和回调契约。

- `engine/config/content/`：通用只读游戏玩法配置的资源清单、文档、快照与构建实现。
- `engine/config/user/`：用户设置的运行时实现与持久化 Store。
- `engine/bootstrap/`：`AppConfig`、`RuntimeSettings`、`BootstrapOutput` 及两阶段启动解析；Bootstrap 消费 `config::UserConfigData`，配置模块不再反向依赖启动编排。

## 启动与内容加载

```text
Bootstrap
  content_registry.json（解析一次） -> Application 持有 ContentRegistry
  AppConfig -> UserConfigService -> renderer -> BuiltinResources
  BuiltinResources -> LocalizationManager -> 项目 preload manifest（全部条目必需）
  Application -> SceneRuntimeContext（向 Scene 提供只读 registry、逻辑画布与 Builtin Asset Cache）

GameContentLoader
  SceneRuntimeContext::content_registry() -> manifests.required.configs -> ConfigLoadPipeline
    -> ConfigManifestLoader
    -> ConfigDocumentLoader
    -> ConfigSnapshotBuilder
  全部资源成功注册 -> ConfigService::publish(snapshot)
```

Bootstrap 不解析或发布游戏玩法配置。内容加载、资源组装或资源注册失败时，已部分提交的内容会被清除，`ConfigService` 保持未初始化。成功加载后的普通 `reset()` 只释放加载器的临时状态，保留已发布快照和资源；新一轮 `start()` 会先清空当前内容，再从空状态加载。当前不支持热重载或在重载失败时保留旧内容。

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

FPS 上限会立即应用到软件限帧器，VSync 会保存为下一次启动使用的值并显示重启提示。如果 VSync 又切回本次启动实际采用的值，待重启状态会自动取消。设置表单在单个纵向滚动区内按显示、音频和通用分组，标题、状态消息及 Save/Back 操作保持固定。具体布局与可见项见[设置面板](../../reference/ui/settings-panel.md#settings-panel)。

## 通用游戏玩法配置

入口是 `manifests.required.configs` 指向的 `assets/configs/manifests/config_manifest.json`：

```json
{
  "schema_version": 1,
  "configs": {}
}
```

`configs` 是命名空间到文档路径的映射。当前映射为空；未来角色、技能、关卡等游戏玩法文档在这里注册，由内容加载阶段统一读取。资源清单自身是严格结构约定；被引用的文档根节点可为 object、array、string、boolean 或 number，但不能为 `null`。

根值使用命名空间配置键注册；对象字段递归展开；数组下标使用无补位 `.0/.1/...` 组成部分。例如：

```json
{
  "difficulty": { "enemy_health_scale": 1.0 },
  "spawn_points": [{ "x": 100, "y": 200 }]
}
```

若命名空间为 `game`，可访问 `game.difficulty.enemy_health_scale` 与 `game.spawn_points.0.x`。命名空间和对象字段组成部分必须符合 `[A-Za-z0-9_]+`。重复 JSON 属性、任意层级的 `null`、非法组成部分和完整配置键冲突都会在快照发布前失败，并保留配置路径、JSON 路径指针与首次与重复声明来源。

`ConfigService` 不解析文件或 JSON，只提供 `publish`、`contains`、六类标量/几何 getter 及对应数组 getter。访问失败会返回 `expected` 并按错误类型去重记录日志；不暴露原始 JSON，也不提供 optional、隐式回退值、热重载或业务专用接口。
