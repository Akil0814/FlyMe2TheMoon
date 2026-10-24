# 本地化与文本

本文说明如何查询翻译、切换语言，以及如何使用文字测量和纹理。翻译视图与缓存纹理都是借用结果，理解其失效时机后再决定是否长期保存。

`ELYSIA_LOCALIZATION` 提供翻译查询、语言切换、文本测量和纹理生成。调用前由应用与内容加载流程准备语言清单、翻译和字体；游戏不要自行初始化内部管理器。

先在[资源指南](../content/resources.md)的国际化清单中登记语言与文件。`assets/i18n/en/base.json` 可写 `{"menu":{"title":"Main menu"}}`，通过 `menu.title` 查询；其他语言目录使用相同键。翻译叶节点必须是字符串，不使用数组或数值作为文本。语言标识用 en、ja、ko、zh-Hans、zh-Hant 等清单中的值，和文档目录的 zh-CN 命名不是同一套用途。

## 查询与切换语言

以下函数可在游戏设置操作中调用，语言标识应来自 `supported_languages()`，不要从目录命名猜测项目一定支持哪些语言。

```cpp
#include "engine/localization/localization_service.h"
#include "engine/tools/logger.h"

#include <string>
#include <expected>
#include <utility>

bool change_language(std::string language)
{
    elysia::localization::LocalizationService* service = ELYSIA_LOCALIZATION;
    const std::expected<void, elysia::localization::LocalizationFailure> result =
        service->set_language(std::move(language));
    if (!result)
    {
        // 记录完整失败及原始来源；返回 false 时不能提示语言已切换。
        const elysia::localization::LocalizationFailure& failure = result.error();
        elysia::tools::Logger::instance()->log_stream(
            elysia::tools::LogLevel::Error, "localization",
            [&](std::ostream& output)
            {
                output << elysia::core::format_failure_diagnostic(
                    failure.diagnostic, failure.error_code(), "localization");
            }, failure.diagnostic.origin);
        return false;
    }
    return true;
}

// 在服务可用后调用，避免在全局变量初始化期间访问服务。
std::string menu_title()
{
    // tr 返回借用视图，复制为 string 后才可独立保留这次查询结果。
    return std::string(ELYSIA_LOCALIZATION->tr("menu.title"));
}
```

`set_language` 可能因依赖未初始化、不支持的语言或加载失败返回错误，不能假定请求已生效。它改变本地化运行状态，不等于保存用户配置；需要持久化的语言设置通过[设置场景](../builtin-scenes/settings.md)或[用户配置服务](../content/configuration.md)完成。

查找先尝试当前语言，再尝试项目默认语言；`engine.` 键还可回退到内置翻译。未找到时返回键本身，未初始化时也返回键，并不抛出“缺少翻译”异常。

`tr()` 返回 `string_view`，不拥有字符串；缺失时甚至可能借用传入翻译键的存储。不要保存临时字符串生成的翻译键所得到的视图。长期保存请复制；语言变化后重新查询，旧文本不会自动变成新语言。

## 测量与纹理

```cpp
#include "engine/localization/localization_service.h"

// 在字体、渲染器和本地化服务准备好后执行。
elysia::localization::LocalizedTextStyle style;
style.wrap_width = 300;
int width = 0;
int height = 0;
if (!ELYSIA_LOCALIZATION->measure_raw_text("Hello", style, width, height))
{
    return; // 放在返回 void 的调用方函数中，处理字体或测量失败。
}
SDL_Texture* texture = ELYSIA_LOCALIZATION->get_raw_text_texture("Hello", style);
if (!texture)
{
    return; // 不提交无效纹理。
}
// texture 是缓存借用指针，交给既有绘制流程使用，不自行销毁。
```

`get_text_texture(key, style)` 使用翻译键；`get_raw_text_texture(text, style)` 使用原文。样式包含排版角色、可选字体来源、颜色和换行宽度。获取失败可能返回空纹理，测量失败返回 false。

缓存纹理由引擎持有，不能调用 `SDL_DestroyTexture()`。成功切换语言会清空文本纹理缓存，字体更新或清理也可能使缓存失效；不要跨这些操作保留裸纹理指针。`font_generation()` 可帮助识别字体变更，但它不是涵盖全部缓存失效原因的通用令牌。

确需独立持有时使用 `create_uncached_raw_text_texture()` 返回的 `CachedTexturePtr`，检查是否为空，并确保纹理在所属渲染器销毁前释放。独立纹理也不会自动随语言或字体变化重新生成。

UI 文本优先使用现有 UI 文本内容接口，让控件处理呈现，而不是在每个按钮里手动管理纹理。

## 参考

- [服务定义](../../../../engine/localization/localization_service.h)
- [资源清单与国际化](../content/resources.md)、[UI 使用](../ui/README.md#create-window)
- [返回使用指南](../README.md)
