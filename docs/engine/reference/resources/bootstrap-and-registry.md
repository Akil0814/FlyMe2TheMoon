# 启动注册与预加载

本文说明内容注册表如何在启动时解析，以及内置资源与项目预加载纹理由谁持有。先建立加载阶段与所有权关系，再检查具体 JSON 字段。

项目根必须包含普通文件 `assets/.elysia_root`，`content_registry.json` 不能替代该标记。默认内容入口为 `assets/content_registry.json`。Bootstrap 在程序启动时仅解析一次，并将解析后的只读 `ContentRegistry` 快照交给引擎 `Application` 持有；场景通过 `SceneRuntimeContext` 借用该快照，内容加载阶段不会再次读取此文件。

根对象必须且只能包含 `bootstrap` 与 `manifests`；解析前会拒绝重复 JSON 属性，未知字段或目标文件不存在都会返回结构化启动失败。

```json
{
  "bootstrap": {
    "app_config": "configs/global/app_config.json",
    "preload_manifest": "configs/manifests/preload_manifest.json"
  },
  "manifests": {
    "required": {
      "configs": "configs/manifests/config_manifest.json",
      "fonts": "configs/manifests/fonts_manifest.json",
      "audio": "configs/manifests/audio_manifest.json",
      "i18n": "configs/manifests/i18n_manifest.json",
      "textures": "configs/manifests/textures_manifest.json",
      "animations": "configs/manifests/animations_manifest.json",
      "effects": "configs/manifests/effects_manifest.json"
    },
    "additional": {}
  }
}
```

## `bootstrap`

| 字段 | 规则 |
| --- | --- |
| `app_config` | 必填 string；按 `assets/` 解析；Bootstrap 读取固定 `AppConfig` 结构约定。 |
| `preload_manifest` | 必填 string；按 `assets/` 解析；用于启动纹理预加载。 |

`bootstrap` 不接受其他字段。通用游戏玩法配置不在启动阶段读取。

## `manifests.required`

`required` 必须是对象，且下列七项全部必填：

| 字段 | 目标 |
| --- | --- |
| `configs` | 通用游戏玩法配置资源清单；由内容加载阶段构建快照。 |
| `fonts` | 核心字体资源清单。 |
| `audio` | 核心 Sound/Music 资源清单。 |
| `i18n` | 国际化资源清单。 |
| `textures` | 核心纹理资源清单。 |
| `animations` | 核心 `Animation` 资源清单。 |
| `effects` | 核心 EffectDefinition 资源清单。 |

每个值必须是 string，按 `assets/` 解析后必须为普通文件。`ContentManifestPipeline` 读取这些声明；`GameContentLoader` 只在 `Atlas`、纹理、字体、音频、`Animation` 与 EffectDefinition 全部成功注册后发布 `configs` 生成的 `ConfigSnapshot`。

## `manifests.additional`

`additional` 可省略；存在时必须是 “模块名 → 模块资源清单路径” 的对象。模块名不决定加载器类型，所有模块都使用同一实体资源包结构约定，并按名称稳定排序。详见[实体内容模块](entity-content.md)。

## `preload_manifest.json`

启动预加载读取显式的 `textures` 条目；独立示例不需要项目 Logo，因此使用空数组：

```json
{
  "textures": []
}
```

需要项目启动纹理时，每项都必须包含稳定资源键和相对文件名；路径基于 `assets/preload/`。资源清单中列出的每个纹理都是必需资源；文件缺失、无法解码或无法创建 SDL texture 都会使第二启动阶段失败，且不会发布部分缓存。

`assets/engine` 由 `BuiltinResources` 单例管理，`Application` 在渲染器建立后调用 `initialize()` 事务式加载：七张 Engine 纹理、五套配置字号字体、五语 `engine.json`、两套动画和一首内建音乐都常驻到 `Application` 关闭流程，且不注册到项目 `ResourceManager`。

固定资源通过 `BuiltinTextureId`、`BuiltinFontId`、`BuiltinAnimationId`、`BuiltinSoundId` 和 `BuiltinMusicId` 等分类枚举查询；稳定字符串名称只用于日志和外部资源保留名校验。其中 `.elysia_engine_required` 是必需根标记，`OFL.txt` 仅为许可证文件。`GameContentLoader` 清理或重新加载项目内容不会影响内置资源。

`BuiltinResources` 复用 `Singleton<T>`，`BuiltinResources::instance()` 返回同一个实例指针；首次访问只构造空门面，不加载 SDL 资源。引擎和项目代码均可包含资源头文件按需查询，无需通过注册上下文、场景构造函数或 `SceneRuntimeContext` 传递。`IGameModule::register_scenes(SceneManager&)` 只负责场景注册。

`Application` 负责单例的运行期生命周期：先关闭 `SceneManager`、本地化、字体等使用者，再显式调用 `BuiltinResources::shutdown()`，最后销毁渲染器并关闭 TTF 和 mixer。`shutdown()` 可重复调用，清理后可以重新初始化；正常释放不依赖进程退出时的静态析构。运行期只支持一个活动 `Application`。

`BootstrapTextureCache` 仅持有项目 preload 资源清单中的启动纹理。`find_preload_texture()` 返回可空的借用指针且查询本身不记录日志；其有效期截止到 `StartupLoadingScene::on_exit()`，`Application` 关闭流程仍保留幂等兜底释放。

## 内置音乐播放

`BuiltinResources` 只负责内置资源的持有与查询，不提供播放或音量接口。内置音乐由独立单例 `BuiltinMusicPlayer` 播放；调用方显式包含 `engine/builtin/audio/builtin_music_player.h` 后访问。首次访问不创建 SDL 资源，`Application` 在内置资源加载后初始化播放器。

播放器与项目音频共用 mixer 设备，但拥有独立轨道，只按 `BuiltinMusicId` 查询内置池，不通过公用资源池查找或回退。`play(id, loops, fade_in)` 使用 SDL_mixer 原生渐入，默认循环播放，未指定或非正时长立即播放；Intro 使用 1500 毫秒渐入，交接给 Realm 时保留当前播放。`stop()` 立即停止并解绑，不支持渐出或播放队列。

主音量和音乐音量由 `Application` 从统一用户设置同步给播放器；修改音量立即生效，并保留渐入进度。无效 ID 或未初始化的播放请求返回失败且不影响当前播放；有效请求（包括相同音乐）重新开始播放，底层播放失败则停止并解绑。

`Application` 先关闭场景，再停止并销毁内置播放器的轨道，随后释放内置资源，最后关闭 mixer。内置资源显式重初始化前及关闭流程时也会停止并解绑内置播放；项目内容清理不影响内置音乐。
