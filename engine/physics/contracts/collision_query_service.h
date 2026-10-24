#pragma once

#include "../collision/collision_query.h"

#include <optional>
#include <vector>

namespace elysia::physics
{
class ICollisionQueryService
{
public:
    virtual ~ICollisionQueryService() = default;

    [[nodiscard]] virtual std::optional<CollisionQueryHit> raycast(
        const RayCastQuery& query
    ) const = 0;

    [[nodiscard]] virtual std::optional<CollisionQueryHit> segment_cast(
        const SegmentCastQuery& query
    ) const = 0;

    virtual void raycast_all(
        const RayCastQuery& query,
        std::vector<CollisionQueryHit>& out_hits) const = 0;

    virtual void segment_cast_all(
        const SegmentCastQuery& query,
        std::vector<CollisionQueryHit>& out_hits) const = 0;

    virtual void overlap_aabb(
        const AabbOverlapQuery& query,
        std::vector<CollisionOverlapQueryHit>& out_hits) const = 0;

    virtual void overlap_circle(
        const CircleOverlapQuery& query,
        std::vector<CollisionOverlapQueryHit>& out_hits) const = 0;

    [[nodiscard]] virtual std::optional<CollisionQueryHit> sweep_aabb(
        const AabbSweepQuery& query) const = 0;
};
}
