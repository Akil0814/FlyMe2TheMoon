#pragma once
#include "../../core/geometry/vector2.h"
#include <cstdint>
#include <variant>
namespace elysia::physics
{
enum class BodyType : std::uint8_t
{
    Static,
    Kinematic,
    Dynamic
};
struct DensityMass
{
    bool operator==(const DensityMass &) const = default;
};
struct ExplicitMass
{
    float mass = 1; // kg; scale the density-derived inertia with mass
    bool operator==(const ExplicitMass &) const = default;
};
struct BodyMassData
{
    float mass = 1;                       // kg
    elysia::core::Vector2 local_center{}; // EU relative to the body origin
    float rotational_inertia = 0;         // kg*EU^2 about the center of mass
    bool operator==(const BodyMassData &) const = default;
};
using MassProperties = std::variant<DensityMass, ExplicitMass, BodyMassData>;
struct BodyDefinition
{
    BodyType type = BodyType::Dynamic;
    MassProperties mass_properties = DensityMass{};
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
    elysia::core::Vector2 local_center{}, world_center{}; // EU; committed center of mass
};
struct PhysicsPose
{
    elysia::core::Vector2 position{};
    float angle = 0;
};
} // namespace elysia::physics
