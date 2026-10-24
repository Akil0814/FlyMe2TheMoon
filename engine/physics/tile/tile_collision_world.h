#pragma once

#include "../collision/collider.h"
#include "../collision/collision_target.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace elysia::physics
{
enum class TileCollisionType : std::uint8_t
{
    Empty,
    Block,
    Overlap,
    OneWay
};

enum class TileOutOfBoundsPolicy : std::uint8_t
{
    Block,
    Empty
};

struct TileCollisionCell
{
    TileCollisionType type = TileCollisionType::Empty;
    CollisionFilter filter{};
    std::optional<OneWayCollision> one_way;
    PhysicsMaterial material{};
    std::string_view tag{};
};

class ITileCollisionWorld
{
public:
    virtual ~ITileCollisionWorld() = default;

    [[nodiscard]] virtual elysia::core::Vector2 world_origin() const noexcept = 0;
    [[nodiscard]] virtual elysia::core::Vector2 tile_size() const noexcept = 0;
    [[nodiscard]] virtual int columns() const noexcept = 0;
    [[nodiscard]] virtual int rows() const noexcept = 0;
    [[nodiscard]] virtual TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept = 0;
    [[nodiscard]] virtual TileCollisionCell cell_at(TileCoordinate coordinate) const noexcept = 0;
};
}
