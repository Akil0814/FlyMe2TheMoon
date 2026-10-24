#pragma once

#include "../../core/geometry/rect.h"
#include "../../core/geometry/vector2.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace elysia::physics
{
struct BoxShape
{
    elysia::core::Vector2 local_center{};
    elysia::core::Vector2 half_extents{};
    float local_angle = 0;
    static BoxShape from_rect(const elysia::core::Rect &rect)
    {
        return {rect.center(), rect.size() * 0.5f, 0};
    }
    elysia::core::Rect local_bounds() const
    {
        const float c = std::abs(std::cos(local_angle)), s = std::abs(std::sin(local_angle));
        const elysia::core::Vector2 bounds{c * half_extents.x + s * half_extents.y,
                                           s * half_extents.x + c * half_extents.y};
        return {local_center - bounds, bounds * 2};
    }
    bool operator==(const BoxShape &) const noexcept = default;
};

struct CircleShape
{
    elysia::core::Vector2 local_center{};
    float radius = 0.0f;
    bool operator==(const CircleShape &) const noexcept = default;
};

struct CapsuleShape
{
    elysia::core::Vector2 local_first{}, local_second{};
    float radius = 0;
    bool operator==(const CapsuleShape &) const noexcept = default;
};
inline constexpr std::size_t kMaxPolygonVertices = 8;
struct ConvexPolygonShape
{
    std::array<elysia::core::Vector2, kMaxPolygonVertices> local_vertices{};
    std::uint8_t vertex_count = 0;
    bool operator==(const ConvexPolygonShape &) const noexcept = default;
};
struct SegmentShape
{
    elysia::core::Vector2 local_first{}, local_second{};
    bool operator==(const SegmentShape&) const noexcept = default;
};
using ColliderShape = std::variant<BoxShape, CircleShape, CapsuleShape, ConvexPolygonShape, SegmentShape>;
} // namespace elysia::physics
