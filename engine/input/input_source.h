#pragma once
#include <compare>
#include <cstdint>
namespace elysia::input
{
enum class InputCancelReason
{
    FocusLost,
    Suppressed,
    Unbound,
    Paused,
    Unavailable,
    Overflow,
    SourceChanged
};
enum class InputSourceKind
{
    None,
    Keyboard,
    Mouse,
    Gamepad
};
struct InputSourceId
{
    std::uint64_t value = 0;
    InputSourceKind kind = InputSourceKind::None;
    constexpr auto operator<=>(const InputSourceId &) const = default;
    [[nodiscard]] constexpr bool is_gamepad() const
    {
        return kind == InputSourceKind::Gamepad;
    }
    [[nodiscard]] constexpr bool is_keyboard() const
    {
        return kind == InputSourceKind::Keyboard;
    }
    [[nodiscard]] constexpr bool is_mouse() const
    {
        return kind == InputSourceKind::Mouse;
    }
    static constexpr InputSourceId keyboard()
    {
        return {1, InputSourceKind::Keyboard};
    }
    static constexpr InputSourceId mouse()
    {
        return {1, InputSourceKind::Mouse};
    }
    static constexpr InputSourceId gamepad(std::uint32_t instance)
    {
        return {instance, InputSourceKind::Gamepad};
    }
};
struct KeyboardPartitionId
{
    std::uint64_t value = 0;
    constexpr auto operator<=>(const KeyboardPartitionId &) const = default;
};
struct LocalPlayerId
{
    std::uint64_t value = 0;
    constexpr auto operator<=>(const LocalPlayerId &) const = default;
};
inline constexpr LocalPlayerId PrimaryLocalPlayer{1};
} // namespace elysia::input
