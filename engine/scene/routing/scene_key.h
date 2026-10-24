#pragma once

#include <cstdint>

namespace elysia::scene
{
using SceneKey = std::uint32_t;

namespace SceneKeys
{
inline constexpr SceneKey Invalid = 0;

inline constexpr SceneKey GameBegin = 1;
inline constexpr SceneKey GameEnd = 999;

inline constexpr SceneKey ElysiaRealm = 1111;

// This value is a range marker, not a usable scene key. Engine-owned
// scenes occupy the values strictly above it.
inline constexpr SceneKey EngineMarker = 0xFFFF0000u;
inline constexpr SceneKey EngineBegin = EngineMarker + 1u;

[[nodiscard]] constexpr bool is_game(SceneKey key) noexcept
{
    return key >= GameBegin && key <= GameEnd;
}

[[nodiscard]] constexpr bool is_engine(SceneKey key) noexcept
{
    return key >= EngineBegin;
}

[[nodiscard]] constexpr bool is_engine_owned(SceneKey key) noexcept
{
    return is_engine(key) || key == ElysiaRealm;
}

[[nodiscard]] constexpr bool is_supported(SceneKey key) noexcept
{
    return is_game(key) || is_engine_owned(key);
}

[[nodiscard]] constexpr bool is_reserved(SceneKey key) noexcept
{
    return key != Invalid && !is_supported(key);
}
}

}
