# 核心资源清单

本文说明核心字体、音频、纹理和本地化清单的字段规则。资源键决定注册身份，文件路径决定加载位置；两者不能互相替代。

核心资源由 `content_registry.json` 的 `manifests.required` 引用。动画和 `Effect` 见[动画与特效](animation-and-effects.md)。

所有核心显式资源键都按点 `.` 拆分，每个组成部分必须匹配 `[A-Za-z0-9_]+`。例如 `ui.moon` 合法，`ui..moon`、`.ui`、`ui-moon` 和 `界面.moon` 非法。

## 字体

```json
{
  "fonts": [
    { "key": "ui.latin", "file": "fusion-pixel.ttf" }
  ]
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `fonts` | array<object> | 必填且非空 |
| `fonts[].key` | string | 必填、非空、合法点分资源键 |
| `fonts[].file` | string | 必填、非空，基于 `assets/fonts/` |

字体资源清单只描述项目字体族文件，不保存字号。字号由 `Application` 字体设置统一解析；项目未提供覆盖值时使用引擎默认 Typography Profile 和 20pt 浮动数字。每个字体条目根据最终项目字号集合生成 Font 请求，资源键为 `<font key>.<size>`，例如 `ui.latin.30`。若 `Application` 未选择任何项目字体来源，则不会生成项目字体请求。派生的数字尺寸也通过统一资源键构造器校验。字体文件的读取与字体创建在资源提交阶段完成。

字体条目在数组中的 JSON 路径指针会进入 `ResourceOrigin`。相同派生 Font 资源键即使来自不同字体条目，也会在请求计划的 Font 注册表查重阶段报告首次与重复声明两个来源。

## 音频

```json
{
  "sounds": {
    "system.confirm": { "path": "system/confirm.wav" }
  },
  "music": {
    "scene.main": { "path": "scene/main.ogg" }
  }
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `sounds` | object | 必填；属性名成为 Sound 资源键 |
| `music` | object | 必填；属性名成为 Music 资源键 |
| 条目 `path` | string | 必填；基于 `assets/audio/` |

两个对象都可为空。每个属性名必须是合法点分资源键，条目对象只接受 `path`。资源清单会拒绝重复 JSON 对象属性；文件读取和解码错误由后续音频加载阶段报告。

Sound 与 Music 是不同注册表，因此二者使用相同字符串资源键合法；同一注册表内与模块 Audio 冲突则失败，并报告两个完整来源。

## 纹理

```json
{
  "textures": {
    "ui.moon": { "path": "ui/moon.png" }
  }
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `textures` | object | 必填 |
| 属性名 | string | 合法点分资源键，直接成为 Texture 资源键 |
| 条目 `path` | string | 必填；基于 `assets/textures/` |

条目对象只接受 `path`。请求生成要求解析结果是普通文件，目录不能作为核心 Texture 条目。资源清单会拒绝重复 JSON 对象属性。

核心 Texture 与所有模块 Texture 进入同一个 Texture 注册表查重；冲突错误包含两边的配置路径、JSON 路径指针、core/module、能力声明、实体和逻辑名。

## 国际化资源清单

```json
{
  "default_language": "en",
  "languages": ["en", "ja", "ko", "zh-Hans", "zh-Hant"],
  "file": ["base.json"]
}
```

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `default_language` | string | 必填、非空 |
| `languages` | array<string> | 必填且非空 |
| `file` | array<string> | 必填且非空，每项非空 |

语言文件按 `assets/i18n/<language>/<file>` 精确查找。`language` 使用标准 BCP 47 locale（例如 `zh-Hans`），不接受下划线 alias。默认语言不在 `languages` 时，运行时会把它追加为受支持语言。

每个语言目录依次加载 `file` 中的 JSON 并合并翻译表。文件不存在、JSON 无效或翻译数据结构不受支持都会使该语言加载失败。i18n 不进入 Atlas/Animation/Effect/Texture/Font/Sound/Music 的资源键注册表。

## Registry 查重摘要

| 核心配置 | 注册表 | 与模块合并查重 |
| --- | --- | --- |
| 字体 + 固定尺寸 | Font | 是 |
| `sounds` | Sound | 是 |
| `music` | Music | 是 |
| 纹理 | Texture | 是 |
| 动画 `Atlas` | `Atlas` | 是 |
| 动画 | `Animation` | 是 |
| EffectDefinition | `Effect` | 是 |

同一字符串跨注册表合法，例如 `Atlas` 与 `Animation` 通常故意共用动画资源键。只有同一个注册表内重复才失败。
