# 实体内容模块

本文说明附加内容模块如何声明实体资源。模块名用于组织内容，不选择不同加载器；所有模块采用相同格式，资源最终进入相应的统一注册表。

`manifests.additional` 的每项都是一个任意命名的实体资源包。所有包由同一个加载器处理；当前仓库只配置了 `ryougi_sample`，模块名不是代码白名单。实际配置入口见[当前仓库样例](#当前仓库样例)。

## 模块资源清单

下面是假设项目中的角色特效模块，用于说明动画与特效的组合声明。`configs/character/`、`textures/character/` 等路径不是当前仓库素材；采用这种组织时，项目必须自行提供文件。本文后续 Texture/Audio 声明与 `FlyingDemon` 实体也属于假设示例。

```json
{
  "entities": "configs/character/characters_manifest.json",
  "key_namespace": "effect",
  "capabilities": {
    "animations": {
      "texture_root": "textures/character/{id}",
      "config_template": "configs/character/{id}/effect_animation_info.json",
      "frame_prefix_template": "{id}_effects_{animation}{segment_suffix}",
      "layouts": {
        "fighter": "configs/character/layouts/character_effect_animation_layout.json"
      }
    },
    "effects": {
      "config_template": "configs/character/{id}/effect_info.json"
    }
  }
}
```

根对象只能包含 `entities`、`key_namespace`、`capabilities`。三项都必填；`key_namespace` 可以是空字符串，`capabilities` 可以为空对象。允许的能力声明固定为可选的 `animations`、`effects`、`textures`、`audio`；未知字段或能力声明会失败。`effects` 存在时，同一模块必须同时提供 `animations`。

## 能力声明

`animations` 必须包含 `texture_root`、`config_template` 和非空 `layouts`。`config_template` 必须含 `{id}`。若该实体动画配置的 `source_type` 是 `frame_directory`，还必须提供 `frame_prefix_template`；横向序列图不使用此前缀。

`frame_prefix_template` 只允许 `{id}`、`{animation}`、`{segment_suffix}`，必须含前两项；配置存在分段动画时还必须含 `{segment_suffix}`。它只能是文件名前缀，不能包含路径分隔符或 `..`。

```json
{
  "textures": {
    "texture_root": "textures/character/{id}",
    "layout": "configs/character/layouts/character_texture_layout.json"
  },
  "audio": {
    "audio_root": "audio/character/{id}",
    "layout": "configs/character/layouts/character_audio_layout.json"
  }
}
```

Texture 和 Audio 能力声明都要求根目录和布局。Texture 布局可指向文件或非递归目录；目录中的直接文件按路径排序，不含扩展名的文件名参与资源键。Audio 布局的目标必须是普通文件，当前只生成 Sound。

## `{id}` 与路径解析

`texture_root` 与 `audio_root` 中出现 `{id}` 时，会替换全部标记；未出现时自动在末尾追加实体 id。只允许此标记，实体化后的根目录必须存在。

所有 `config_template` 必须显式包含 `{id}`，替换后必须是存在的普通配置文件。`Animation` 的 `layouts` 与 Texture/Audio 的 `layout` 都相对于 `assets/` 解析；布局内的资源路径则相对于实体化后的对应根目录解析。

## 实体资源清单

```json
{
  "entities": [
    {
      "id": "FlyingDemon",
      "enabled": true,
      "animation_layout": "normal"
    }
  ]
}
```

| 字段 | 规则 |
| --- | --- |
| `id` | 必填、唯一、合法资源键组成部分；同时是资源目录标识、模板参数和运行时资源键的首段。当前实体采用 PascalCase 约定。 |
| `enabled` | 可选，默认 `true`；为 `false` 时跳过该实体。 |
| `animation_layout` | 使用 `Animation` 能力声明时必填，且必须匹配该能力声明的 `layouts` 键；若提供也必须是合法组成部分。 |

实体条目只能包含上述字段。`id` 的语法仍是通用的 `[A-Za-z0-9_]+`，PascalCase 是仓库实体命名约定而非额外语法限制。角色显示名继续由独立的 i18n `display_name_key` 控制，不随实体 id 改写。

## 资源键

所有能力声明使用同一个资源键构造器：

```text
base      = <entity id>[.<key_namespace>]
Animation = <base>.<animation>[.<segment index>]
Effect    = <base>.<effect>[.<segment index>]
Texture   = <base>.<texture logical name>[.<directory file stem>]
Sound     = <base>.<audio logical name>
```

以下是上述假设项目的资源键示例；模块名本身不进入资源键，实体 ID 与命名空间才参与构造：

```text
characters / namespace ""      -> RyougiShiki.idle
character_effects / "effect"   -> RyougiShiki.effect.attack_normal.0
```

每个组成部分使用 `[A-Za-z0-9_]+`；点仅用于连接组成部分。空组成部分、横线、空格、非 ASCII 字符和连续/首尾点都非法。分段资源键永不补位；补两位只属于文件系统的目录与文件名前缀。

## 当前仓库样例

| 模块 | 命名空间 | 能力声明 |
| --- | --- | --- |
| `ryougi_sample` | `""` | `animations` |

可以沿以下实际文件阅读加载输入：

1. [content_registry.json](../../../../assets/content_registry.json) 的 `manifests.additional` 引用模块。
2. [content_manifest.json](../../../../assets/configs/examples/ryougi/content_manifest.json) 声明空命名空间、实体清单和动画模板。
3. [entities_manifest.json](../../../../assets/configs/examples/ryougi/entities_manifest.json) 启用 `RyougiShiki`，选择 `fighter` 布局。
4. [animation_layout.json](../../../../assets/configs/examples/ryougi/animation_layout.json) 定义相对实体纹理根的动画路径；[animation_info.json](../../../../assets/configs/examples/ryougi/RyougiShiki/animation_info.json) 定义帧数、帧率、循环和分段。
5. 纹理根为 [textures/examples/ryougi/RyougiShiki](../../../../assets/textures/examples/ryougi/RyougiShiki)，例如生成 `RyougiShiki.idle` 动画键。

样例只声明动画，不意味着通用加载器不再支持 Texture、Audio 或 Effect。多个模块仍可复用同一实体清单；若项目自行声明 `effect` 命名空间的模块，它加载自身动画并据此注册 EffectDefinition，不会自动加载另一模块的本体动画或生成当前样例未声明的特效键。

## 常见失败

- 实体缺少 id、id 重复、含未知字段或组成部分非法；
- 模板缺少 `{id}`、使用未知标记，或解析后的路径不存在；
- `effects` 没有同模块的 `animations`；
- 实体的 `animation_layout` 不在能力声明的 `layouts` 中；
- Texture/Audio 布局或目录纹理不含扩展名的文件名非法；
- 核心和模块在同一资源注册表生成重复资源键。错误会列出首次与重复声明两个完整 `ResourceOrigin`。
