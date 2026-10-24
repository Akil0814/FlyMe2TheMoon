#pragma once

#include "collision_contact.h"

namespace elysia::physics
{
struct RayCastQuery
{
    elysia::core::Vector2 origin{};
    elysia::core::Vector2 direction{};
    float max_distance = 0.0f;
    CollisionFilter filter{};
};

struct SegmentCastQuery
{
    elysia::core::Vector2 start{};
    elysia::core::Vector2 end{};
    CollisionFilter filter{};
};

struct AabbOverlapQuery
{
    elysia::core::Rect bounds{};
    CollisionFilter filter{};
};

struct CircleOverlapQuery
{
    elysia::core::Vector2 center{};
    float radius = 0.0f;
    CollisionFilter filter{};
};

struct AabbSweepQuery
{
    elysia::core::Rect start_bounds{};
    elysia::core::Vector2 displacement{};
    CollisionFilter filter{};
};

struct CollisionOverlapQueryHit
{
    CollisionTarget target{};
    CollisionResponse response = CollisionResponse::Ignore;
};

struct CollisionQueryHit
{
    CollisionTarget target{};
    elysia::core::Vector2 point{};
    elysia::core::Vector2 normal{};
    float distance = 0.0f;
    float fraction = 0.0f;
    CollisionResponse response = CollisionResponse::Ignore;
};

}
