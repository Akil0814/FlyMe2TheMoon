#pragma once

#include "collider.h"

#include <compare>
#include <cstdint>
#include <functional>

namespace elysia::physics
{
struct TileCoordinate
{
    int x = 0;
    int y = 0;

    [[nodiscard]] constexpr bool operator==(const TileCoordinate&) const noexcept = default;

    [[nodiscard]] constexpr std::strong_ordering operator<=> (
        const TileCoordinate& other) const noexcept
    {
        if (const auto y_order = y <=> other.y; y_order != 0)
            return y_order;
        return x <=> other.x;
    }
};

enum class CollisionTargetKind : std::uint8_t
{
    Invalid,
    Collider,
    Tile
};

struct CollisionTarget
{
    CollisionTargetKind kind = CollisionTargetKind::Invalid;
    ColliderId collider = InvalidColliderId;
    TileCoordinate tile{};

    [[nodiscard]] static constexpr CollisionTarget from_collider(
        ColliderId id) noexcept
    {
        return id == InvalidColliderId
            ? CollisionTarget{}
            : CollisionTarget{CollisionTargetKind::Collider, id, {}};
    }

    [[nodiscard]] static constexpr CollisionTarget from_tile(
        TileCoordinate coordinate) noexcept
    {
        return CollisionTarget{
            CollisionTargetKind::Tile,
            InvalidColliderId,
            coordinate
        };
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        switch (kind)
        {
        case CollisionTargetKind::Collider:
            return collider != InvalidColliderId;
        case CollisionTargetKind::Tile:
            return collider == InvalidColliderId;
        case CollisionTargetKind::Invalid:
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr bool operator==(
        const CollisionTarget& other) const noexcept
    {
        if (kind != other.kind)
            return false;
        if (kind == CollisionTargetKind::Collider)
            return collider == other.collider;
        if (kind == CollisionTargetKind::Tile)
            return tile == other.tile;
        return true;
    }

    [[nodiscard]] constexpr std::strong_ordering operator<=> (
        const CollisionTarget& other) const noexcept
    {
        if (const auto kind_order = kind <=> other.kind; kind_order != 0)
            return kind_order;
        if (kind == CollisionTargetKind::Collider)
            return collider <=> other.collider;
        if (kind == CollisionTargetKind::Tile)
            return tile <=> other.tile;
        return std::strong_ordering::equal;
    }
};

struct CollisionTargetHash
{
    [[nodiscard]] std::size_t operator()(
        const CollisionTarget& target) const noexcept
    {
        std::size_t seed = static_cast<std::size_t>(target.kind);
        if (target.kind == CollisionTargetKind::Collider)
            return seed ^ (std::hash<ColliderId>{}(target.collider) << 1u);
        if (target.kind == CollisionTargetKind::Tile)
        {
            seed ^= std::hash<int>{}(target.tile.x) << 1u;
            seed ^= std::hash<int>{}(target.tile.y) << 2u;
        }
        return seed;
    }
};
}
