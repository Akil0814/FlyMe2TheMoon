#include "builtin_resource_ids.h"

#include "../../localization/locale.h"

#include <array>

namespace elysia::builtin
{
namespace
{
template<typename Id, std::size_t Size>
std::string_view name_at(Id id, const std::array<std::string_view, Size>& names) noexcept
{
    const std::size_t index = builtin_resource_index(id);
    return index < names.size() ? names[index] : std::string_view{};
}
}

std::string_view builtin_resource_name(BuiltinTextureId id) noexcept
{
    static constexpr std::array names{
        std::string_view{"engine.brand.elysia.default"},
        std::string_view{"engine.brand.elysia.black"},
        std::string_view{"engine.brand.elysia.black_alpha_inverse"},
        std::string_view{"engine.brand.elysia.light_edge"},
        std::string_view{"engine.brand.elysia.white"},
        std::string_view{"engine.character.sprite.idle"},
        std::string_view{"engine.character.sprite.move"}
    };
    return name_at(id, names);
}

std::string_view builtin_resource_name(BuiltinFontId id) noexcept
{
    static constexpr std::array names{
        std::string_view{"engine.font.latin"},
        std::string_view{"engine.font.zh_hans"},
        std::string_view{"engine.font.zh_hant"},
        std::string_view{"engine.font.ja"},
        std::string_view{"engine.font.ko"}
    };
    return name_at(id, names);
}

std::string_view builtin_resource_name(BuiltinLocaleId id) noexcept
{
    return builtin_locale_name(id);
}

std::string_view builtin_resource_name(BuiltinAnimationId id) noexcept
{
    static constexpr std::array names{
        std::string_view{"engine.character.idle"},
        std::string_view{"engine.character.move"}
    };
    return name_at(id, names);
}

std::string_view builtin_resource_name(BuiltinSoundId) noexcept
{
    return {};
}

std::string_view builtin_resource_name(BuiltinMusicId id) noexcept
{
    static constexpr std::array names{
        std::string_view{"engine.elysia.music"}
    };
    return name_at(id, names);
}

std::string_view builtin_locale_name(BuiltinLocaleId id) noexcept
{
    static constexpr std::array names{
        elysia::localization::kEnglishLocale,
        elysia::localization::kSimplifiedChineseLocale,
        elysia::localization::kTraditionalChineseLocale,
        elysia::localization::kJapaneseLocale,
        elysia::localization::kKoreanLocale
    };
    return name_at(id, names);
}

BuiltinLocaleId builtin_locale_id(std::string_view locale) noexcept
{
    for (std::size_t index = 0;
         index < builtin_resource_index(BuiltinLocaleId::Count);
         ++index)
    {
        const auto id = static_cast<BuiltinLocaleId>(index);
        if (builtin_locale_name(id) == locale)
            return id;
    }
    return BuiltinLocaleId::Count;
}

BuiltinFontId builtin_font_id(BuiltinLocaleId locale) noexcept
{
    switch (locale)
    {
    case BuiltinLocaleId::English:
        return BuiltinFontId::Latin;
    case BuiltinLocaleId::SimplifiedChinese:
        return BuiltinFontId::SimplifiedChinese;
    case BuiltinLocaleId::TraditionalChinese:
        return BuiltinFontId::TraditionalChinese;
    case BuiltinLocaleId::Japanese:
        return BuiltinFontId::Japanese;
    case BuiltinLocaleId::Korean:
        return BuiltinFontId::Korean;
    case BuiltinLocaleId::Count:
    default:
        return BuiltinFontId::Count;
    }
}
}
