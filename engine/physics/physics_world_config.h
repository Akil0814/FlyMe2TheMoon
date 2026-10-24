#pragma once
#include "../core/geometry/vector2.h"
#include <cstdint>
namespace elysia::physics
{
struct PhysicsWorldConfig
{
    std::uint32_t sub_steps = 4;
    float units_per_meter = 100;
    elysia::core::Vector2 gravity{};
    float contact_normal_threshold = 0.5f;
    float restitution_velocity_threshold = 100; // EU/s
    bool enable_sleep = true;
};
} // namespace elysia::physics
