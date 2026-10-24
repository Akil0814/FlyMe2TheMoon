#pragma once

#include "gameplay_collision_types.h"

#include "../../physics/collision/collision_event.h"

namespace elysia::gameplay::collision
{
struct BodyContactEvent
{
    elysia::physics::CollisionEventPhase phase = elysia::physics::CollisionEventPhase::Begin;
    ColliderBinding body{};
    elysia::physics::CollisionTarget other{};
    elysia::physics::CollisionContact contact{};
};

struct PushBoxOverlapEvent
{
    elysia::physics::CollisionEventPhase phase = elysia::physics::CollisionEventPhase::Begin;
    ColliderBinding first{};
    ColliderBinding second{};
    elysia::physics::CollisionOverlap overlap{};
};

struct HitOverlapEvent
{
    elysia::physics::CollisionEventPhase phase = elysia::physics::CollisionEventPhase::Begin;
    HitBoxBinding hit_box{};
    ColliderBinding hurt_box{};
    elysia::physics::CollisionOverlap overlap{};
};

struct SensorOverlapEvent
{
    elysia::physics::CollisionEventPhase phase = elysia::physics::CollisionEventPhase::Begin;
    ColliderBinding sensor{};
    ColliderBinding body{};
    elysia::physics::CollisionOverlap overlap{};
};
}
