# 资源加载与使用

本文面向准备游戏素材与内容清单的开发者。先说明资源如何从声明进入加载流程，再介绍各类配置与查询方式；配置示例遵循当前格式，不表示可任意增加字段。

资源由应用和内容加载流程统一加载。游戏代码使用 `ELYSIA_RESOURCES` 查询已加载的资源，不自行初始化或关闭资源管理器。先完成[游戏初始化](../game-initialization.md)，通过[启动加载场景](../builtin-scenes/startup-loading.md)加载内容，成功后再进入使用这些资源的场景。

## 最小使用流程

1. 在下文对应资源清单中声明资源，将文件放在规定的资源根目录。
2. 确保 `assets/content_registry.json` 引用这些资源清单。修改注册表、启动配置或预加载声明后重启应用；注册表在启动时解析，内容加载不会重新读取它。
3. 加载成功后按资源键查询并处理空结果。纹理交给[游戏对象](../scene/game-objects.md)绘制，音频交给[音频服务](../systems/audio.md)播放。修改正式内容清单也不会自动热重载；开发时重启应用是最直接的验证方式。

下面的辅助函数放在游戏代码中，在内容加载成功后调用；调用方决定缺少图片时跳过显示还是进入错误流程。

```cpp
#include "engine/resources/resource_service.h"
#include "engine/tools/logger.h"

SDL_Texture* find_icon()
{
    // 查询已完成加载的资源键，不在绘制或更新循环里临时加载文件。
    SDL_Texture* texture = ELYSIA_RESOURCES->find_texture("ui.moon");
    if (!texture)
        ELYSIA_LOG_ERROR("resources", "Missing texture: ui.moon");
    return texture; // 借用，不调用 SDL_DestroyTexture。
}
```

| 查询 | 返回与使用 |
| --- | --- |
| `find_texture(key)` | 可空 `SDL_Texture` 指针，用于绘制命令 |
| `find_atlas(key)` | 可空 const `Atlas` 指针，包含帧信息 |
| `has_font(key)` / `find_font(key)` | 检查或借用已注册字号字体，注意资源键带字号 |
| `find_sound(key)` / `find_music(key)` | 可空 MIX_Audio 指针；普通播放使用 `AudioService` |

以上均为借用。加载失败或资源清理后不可使用旧指针；场景切换本身不意味着资源一定清空，但重新开始内容加载会先清除旧项目内容。不要在依赖旧资源的对象仍使用它们时启动重载，也不要假设失败会保留上一套资源。

## 准备内容入口

项目根放置普通文件 `assets/.elysia_root`，并准备 `assets/content_registry.json`。以下注册表指向项目实际提供的清单文件；七项 required 入口都应保留，文件内容按各自格式编写。

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

启动时解析注册表，后续内容加载借用同一快照。修改入口后重启应用。路径解析、必填字段和预加载格式见[注册表参考](../../reference/resources/bootstrap-and-registry.md)。

没有启动 Logo 时，预加载清单可写 `{"textures": []}`。需要 Logo 时，将图片放在 `assets/preload/logo.png`，写入如下清单，再把 `project.logo` 交给启动加载 Payload 的 `project_logo`：

```json
{
  "textures": [{ "key": "project.logo", "file": "logo.png" }]
}
```

预加载纹理属于启动缓存，不会自动进入项目纹理服务。清单中的图片全部必需，缺少文件或解码失败会终止对应启动阶段。

## 添加第一张项目纹理

1. 把图片放到 `assets/textures/ui/moon.png`。
2. 在注册表 `required.textures` 指向的文件中加入以下条目。
3. 重启并等待内容加载成功，再调用本页的 `find_icon()`。查询使用 `ui.moon`，不是文件路径。

```json
{
  "textures": {
    "ui.moon": { "path": "ui/moon.png" }
  }
}
```

资源键使用点分名称，组成部分使用字母、数字或下划线；同一注册表内不能重复。其他核心资源按[核心资源清单](../../reference/resources/core-resource-manifests.md)声明：字体文件放在 `assets/fonts/`，音频放在 `assets/audio/`，翻译放在 `assets/i18n/<language>/`。

字体声明本身不指定字号；在应用描述中选择项目字体来源后，由字体设置生成所需字号资源。音频播放使用[音频服务](../systems/audio.md)，翻译文本使用[本地化服务](../systems/localization.md)，不自行释放查询所得资源。

## 组织实体内容资源包

当多个角色共享同一种资源布局时，在 `manifests.additional` 中增加“模块名 → 模块清单路径”。模块名只是分类，不决定加载器类型。

按以下顺序准备一套内容：

1. 模块清单指定实体清单、资源键命名空间及所需能力。
2. 实体清单列出实体 id；使用动画时为每个实体选择已声明的布局。
3. 布局描述资源位置，实体配置描述帧数、播放参数或特效映射。
4. 放置对应文件，加载后使用生成的资源键，例如 `RyougiShiki.idle`。

模块、实体和布局的配套 JSON 见[实体内容格式](../../reference/resources/entity-content.md)。不要向实体条目增加显示名或战斗数值；这些属于游戏配置。特效能力必须与同模块的动画能力配套，详见[动画与特效使用](../systems/animation-and-effects.md#资源配置)。

失败时先查看日志中给出的配置路径与 JSON 位置，再检查模板替换后的文件、布局名及资源键。重复键会同时报告两处声明；不要靠修改加载顺序覆盖另一条资源。

## 加载失败与生命周期

项目 preload 条目全部必需；配置错误、文件缺失、解码或注册失败应中止进入依赖该内容的场景。启动 Logo 查询可选与 preload 文件是否必需是不同规则，见[启动加载](../builtin-scenes/startup-loading.md)。preload 纹理不通过 `ResourceService` 查询，其借用期在 `StartupLoadingScene` 退出时结束。

正常加载入口由 `StartupLoadingScene` 驱动，不在普通对象逐帧更新里反复加载。项目资源加载失败会清理已部分提交内容；成功后加载器的普通 reset 只清理临时状态，新一轮 start 才开启重新加载。

内建资源和项目资源分开管理，项目清理不会清除引擎内建资源。不要用项目资源键覆盖内建保留名称，不自行释放服务返回的字体、音频、纹理或图集。

常见错误是把文件路径当作资源键、遗漏字号后缀、把 Sound 资源键当 Music 资源键、资源未加载就查询，以及在重载后继续使用旧指针。相同字符串在不同资源类型中可以共存，同一类型重复注册会失败。

## 相关指南

- [动画与特效配置及播放](../systems/animation-and-effects.md)、[配置读取](configuration.md)、[本地化](../systems/localization.md)
- [资源服务接口](../../../../engine/resources/resource_service.h)、[可选架构参考](../../architecture/subsystems/resources/README.md)
- [返回使用指南](../README.md)
