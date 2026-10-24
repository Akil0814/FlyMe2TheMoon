#pragma once
#include "../../core/geometry/vector2.h"
#include <cstdint>
namespace elysia::physics
{
enum class BodyType : std::uint8_t
{
    Static,
    Kinematic,
    Dynamic
};
enum class MassPolicy : std::uint8_t
{
    FromDensity,
    ExplicitMass
};
struct BodyDefinition
{
    BodyType type = BodyType::Dynamic;
    MassPolicy mass_policy = MassPolicy::FromDensity;
    float mass = 1;                        // kg
    elysia::core::Vector2 velocity{};      // EU/s
    float angle = 0, angular_velocity = 0; // radians, radians/s, clockwise in Y-down
    float gravity_scale = 1, linear_damping = 0, angular_damping = 0;
    bool fixed_rotation = true, enable_sleep = true, enabled = true, bullet = false;
};
struct BodyState
{
    elysia::core::Vector2 position{}, velocity{};
    float angle = 0, angular_velocity = 0, mass = 0, rotational_inertia = 0;
    bool awake = false, enabled = false;
};
struct PhysicsPose
{
    elysia::core::Vector2 position{};
    float angle = 0;
};
} // namespace elysia::physics
