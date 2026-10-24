#pragma once
#include "../core/geometry/vector2.h"
#include "physics_object_handle.h"
namespace elysia::physics
{
struct JointHandle
{
    std::uint64_t value = 0;
    bool is_valid() const noexcept
    {
        return value != 0;
    }
    bool operator==(const JointHandle &) const noexcept = default;
};
struct DistanceJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{};
    float length = 100;
    bool spring = false;
    float frequency_hz = 2, damping_ratio = 0.7f;
    bool collide_connected = false;
};
struct RevoluteJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{};
    float reference_angle = 0, lower_angle = 0, upper_angle = 0;
    bool enable_limit = false, enable_motor = false;
    float motor_speed = 0, max_motor_torque = 0; // rad/s, kg*EU^2/s^2
    bool collide_connected = false;
};
struct JointState
{
    elysia::core::Vector2 anchor_first{}, anchor_second{}, reaction_force{};
    float reaction_torque = 0;
};
} // namespace elysia::physics
