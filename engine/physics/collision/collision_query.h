#pragma once
#include "../body/body_definition.h"
#include "../physics_object_handle.h"
#include "collision_contact.h"
namespace elysia::physics
{
struct CollisionQueryFilter
{
    CollisionFilter collision{};
    PhysicsObjectHandle ignored_object{};
    bool include_sensors = true;
};
struct RayCastQuery
{
    elysia::core::Vector2 origin{}, direction{};
    float max_distance = 0;
    CollisionQueryFilter filter{};
};
struct SegmentCastQuery
{
    elysia::core::Vector2 start{}, end{};
    CollisionQueryFilter filter{};
};
struct ShapeOverlapQuery
{
    ColliderShape shape{};
    PhysicsPose pose{};
    CollisionQueryFilter filter{};
};
struct ShapeCastQuery
{
    ColliderShape shape{};
    PhysicsPose pose{};
    elysia::core::Vector2 displacement{};
    CollisionQueryFilter filter{};
};
struct CollisionOverlapQueryHit
{
    CollisionTarget target{};
    CollisionResponse response = CollisionResponse::Ignore;
};
struct CollisionQueryHit
{
    CollisionTarget target{};
    elysia::core::Vector2 point{}, normal{};
    float distance = 0, fraction = 0;
    CollisionResponse response = CollisionResponse::Ignore;
    bool initial_overlap = false;
};
} // namespace elysia::physics
