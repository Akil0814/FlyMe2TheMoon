#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace elysia::builtin
{
enum class BuiltinTextureId : std::uint8_t
{
    ElysiaDefault,
    ElysiaBlack,
    ElysiaBlackAlphaInverse,
    ElysiaLightEdge,
    ElysiaWhite,
    EngineCharacterIdle,
    EngineCharacterMove,
    Count
};

enum class BuiltinFontId : std::uint8_t
{
    Latin,
    SimplifiedChinese,
    TraditionalChinese,
    Japanese,
    Korean,
    Count
};

enum class BuiltinLocaleId : std::uint8_t
{
    English,
    SimplifiedChinese,
    TraditionalChinese,
    Japanese,
    Korean,
    Count
};

enum class BuiltinAnimationId : std::uint8_t
{
    EngineCharacterIdle,
    EngineCharacterMove,
    Count
};

enum class BuiltinSoundId : std::uint8_t
{
    Count
};

enum class BuiltinMusicId : std::uint8_t
{
    ElysianRealm,
    Count
};

template<typename Id>
[[nodiscard]] constexpr std::size_t builtin_resource_index(Id id) noexcept
{
    return static_cast<std::size_t>(id);
}

[[nodiscard]] std::string_view builtin_resource_name(BuiltinTextureId id) noexcept;
[[nodiscard]] std::string_view builtin_resource_name(BuiltinFontId id) noexcept;
[[nodiscard]] std::string_view builtin_resource_name(BuiltinLocaleId id) noexcept;
[[nodiscard]] std::string_view builtin_resource_name(BuiltinAnimationId id) noexcept;
[[nodiscard]] std::string_view builtin_resource_name(BuiltinSoundId id) noexcept;
[[nodiscard]] std::string_view builtin_resource_name(BuiltinMusicId id) noexcept;

[[nodiscard]] std::string_view builtin_locale_name(BuiltinLocaleId id) noexcept;
[[nodiscard]] BuiltinLocaleId builtin_locale_id(std::string_view locale) noexcept;
[[nodiscard]] BuiltinFontId builtin_font_id(BuiltinLocaleId locale) noexcept;
}
