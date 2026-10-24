#pragma once

#include "collider_shape.h"
#include "physics_material.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace elysia::physics
{
using ColliderId = std::uint64_t;
using CollisionBits = std::uint32_t;

inline constexpr ColliderId InvalidColliderId = 0;

struct CollisionFilter
{
    bool operator==(const CollisionFilter&) const noexcept = default;
    CollisionBits category = 1u;
    CollisionBits mask = 0xffffffffu;
    std::int32_t group = 0;
};

enum class CollisionResponse : std::uint8_t
{
    Ignore,
    Overlap,
    Block
};

enum class CollisionDetectionMode : std::uint8_t
{
    Discrete,
    Continuous
};

enum class PassThroughDirection : std::uint8_t
{
    None = 0,
    Up = 1u << 0,
    Down = 1u << 1,
    Left = 1u << 2,
    Right = 1u << 3
};

[[nodiscard]] constexpr PassThroughDirection operator|(
    PassThroughDirection first,
    PassThroughDirection second
) noexcept
{
    return static_cast<PassThroughDirection>(
        static_cast<std::uint8_t>(first) |
        static_cast<std::uint8_t>(second)
    );
}

constexpr PassThroughDirection& operator|=(
    PassThroughDirection& first,
    PassThroughDirection second
) noexcept
{
    first = first | second;
    return first;
}

[[nodiscard]] constexpr bool has_pass_through_direction(
    PassThroughDirection directions,
    PassThroughDirection direction
) noexcept
{
    if (direction == PassThroughDirection::None)
        return false;

    const std::uint8_t direction_bits = static_cast<std::uint8_t>(direction);
    return (static_cast<std::uint8_t>(directions) & direction_bits) == direction_bits;
}

struct OneWayCollision
{
    bool operator==(const OneWayCollision&) const noexcept = default;
    PassThroughDirection pass_through = PassThroughDirection::None;
    float tolerance = 0.01f;
};

struct Collider
{
    bool operator==(const Collider&) const noexcept = default;
    ColliderShape shape{AabbShape{}};

    CollisionFilter filter{};
    CollisionResponse response = CollisionResponse::Block;
    CollisionDetectionMode detection_mode = CollisionDetectionMode::Discrete;
    std::optional<OneWayCollision> one_way;
    PhysicsMaterial material{};
    SurfaceDensity density{};
    bool sensor_contributes_mass = false;

    std::string_view tag{};

    bool enabled = true;
};
}
