#pragma once

#include "collision_target.h"

#include "../../core/geometry/vector2.h"

#include <array>
#include <cstdint>

namespace elysia::physics
{
struct CollisionPair
{
    CollisionTarget first{};
    CollisionTarget second{};

    [[nodiscard]] constexpr bool operator==(const CollisionPair&) const noexcept = default;
    [[nodiscard]] constexpr std::strong_ordering operator<=>(
        const CollisionPair&) const noexcept = default;
};

[[nodiscard]] constexpr CollisionPair normalized_collision_pair(
    CollisionTarget first,
    CollisionTarget second) noexcept
{
    return second < first
        ? CollisionPair{second, first}
        : CollisionPair{first, second};
}

struct CollisionManifold
{
    // Normal points from CollisionPair::first to second.
    elysia::core::Vector2 normal{};
    float penetration = 0.0f;
    std::array<elysia::core::Vector2, 2> contact_points{};
    std::uint8_t contact_point_count = 0;
};

struct CollisionOverlap
{
    CollisionPair pair{};
    CollisionManifold manifold{};
};

struct CollisionContact
{
    CollisionPair pair{};
    CollisionManifold manifold{};
    CollisionResponse response = CollisionResponse::Ignore;
    float normal_impulse = 0.0f;
    float tangent_impulse = 0.0f;
};
}
