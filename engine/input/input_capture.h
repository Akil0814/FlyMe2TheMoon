#pragma once

#include <cstdint>

namespace elysia::input
{
enum class InputCapture : std::uint8_t
{
    None = 0,
    Keyboard = 1u << 0,
    Pointer = 1u << 1,
    Gamepad = 1u << 2
};

[[nodiscard]] constexpr InputCapture operator|(InputCapture first, InputCapture second) noexcept
{
    return static_cast<InputCapture>(static_cast<std::uint8_t>(first) | static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr InputCapture operator&(InputCapture first, InputCapture second) noexcept
{
    return static_cast<InputCapture>(static_cast<std::uint8_t>(first) & static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr bool captures_input(InputCapture capture, InputCapture requested) noexcept
{
    return (capture & requested) != InputCapture::None;
}
} // namespace elysia::input
