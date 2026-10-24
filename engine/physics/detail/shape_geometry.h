#pragma once
#include "../body/body_definition.h"
#include "../collision/collider_shape.h"
#include "physics_units.h"
#include <box2d/box2d.h>
#include <optional>
namespace elysia::physics::detail
{
// All geometry is validated in simulation units before invoking native constructors.
struct ShapeGeometry
{
    enum class Kind
    {
        Circle,
        Capsule,
        Polygon,
        Segment
    } kind;
    b2Circle circle{};
    b2Capsule capsule{};
    b2Polygon polygon{};
    b2Segment segment{};
    b2ShapeProxy proxy{};
};
std::optional<ShapeGeometry> shape_geometry(const ColliderShape &, const PhysicsUnits &);
b2AABB shape_bounds(b2ShapeId, b2Transform);
} // namespace elysia::physics::detail
