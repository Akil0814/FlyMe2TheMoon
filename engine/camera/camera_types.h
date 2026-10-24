#pragma once

#include <cstddef>
#include <cstdint>

namespace elysia::camera
{
enum class CameraSlot : std::size_t
{
    Main,
    Cinematic,
    Auxiliary1,
    Auxiliary2,
    Count
};

[[nodiscard]] constexpr bool valid_camera_slot(CameraSlot slot) noexcept
{
    return static_cast<std::size_t>(slot) < static_cast<std::size_t>(CameraSlot::Count);
}

class CameraSlotSet final
{
public:
    constexpr CameraSlotSet() noexcept = default;
    constexpr CameraSlotSet(CameraSlot slot) noexcept
        : _bits(valid_camera_slot(slot)
              ? static_cast<std::uint8_t>(1u << static_cast<std::size_t>(slot))
              : 0),
          _valid(valid_camera_slot(slot))
    {
    }

    [[nodiscard]] static constexpr CameraSlotSet all() noexcept
    {
        CameraSlotSet result;
        result._bits = static_cast<std::uint8_t>(
            (1u << static_cast<std::size_t>(CameraSlot::Count)) - 1u);
        return result;
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return _bits == 0; }
    [[nodiscard]] constexpr bool valid() const noexcept { return _valid; }
    [[nodiscard]] constexpr bool contains(CameraSlot slot) const noexcept
    {
        return valid_camera_slot(slot)
            && (_bits & static_cast<std::uint8_t>(1u << static_cast<std::size_t>(slot))) != 0;
    }

    constexpr CameraSlotSet& operator|=(CameraSlotSet other) noexcept
    {
        _bits = static_cast<std::uint8_t>(_bits | other._bits);
        _valid = _valid && other._valid;
        return *this;
    }

    friend constexpr CameraSlotSet operator|(CameraSlotSet first, CameraSlotSet second) noexcept
    {
        first |= second;
        return first;
    }

private:
    std::uint8_t _bits = 0;
    bool _valid = true;
};

[[nodiscard]] constexpr CameraSlotSet operator|(CameraSlot first, CameraSlot second) noexcept
{
    return CameraSlotSet(first) | CameraSlotSet(second);
}
} // namespace elysia::camera
