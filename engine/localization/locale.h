#pragma once

#include <array>
#include <string_view>

namespace elysia::localization
{
inline constexpr std::string_view kEnglishLocale = "en";
inline constexpr std::string_view kJapaneseLocale = "ja";
inline constexpr std::string_view kKoreanLocale = "ko";
inline constexpr std::string_view kSimplifiedChineseLocale = "zh-Hans";
inline constexpr std::string_view kTraditionalChineseLocale = "zh-Hant";

inline constexpr std::array<std::string_view,5> kBuiltinLocales{
    kEnglishLocale,
    kJapaneseLocale,
    kKoreanLocale,
    kSimplifiedChineseLocale,
    kTraditionalChineseLocale
};

[[nodiscard]] constexpr std::string_view locale_key_segment(
    std::string_view locale) noexcept
{
    if (locale == kEnglishLocale)
        return "en";
    if (locale == kJapaneseLocale)
        return "ja";
    if (locale == kKoreanLocale)
        return "ko";
    if (locale == kSimplifiedChineseLocale)
        return "zh_hans";
    if (locale == kTraditionalChineseLocale)
        return "zh_hant";
    return {};
}
}
