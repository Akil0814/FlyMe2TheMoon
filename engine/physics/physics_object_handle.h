#pragma once

#include <compare>
#include <cstdint>

namespace elysia::physics
{
struct PhysicsObjectHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return value != 0;
    }

    constexpr auto operator<=>(const PhysicsObjectHandle&) const noexcept = default;
};

inline constexpr PhysicsObjectHandle InvalidPhysicsObjectHandle{};
}
