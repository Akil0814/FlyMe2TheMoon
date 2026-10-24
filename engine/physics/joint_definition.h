#pragma once
#include "../core/geometry/vector2.h"
#include "physics_object_handle.h"
#include <variant>
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
struct DistanceJointSettings
{
    float length = 100;
    bool enable_spring = false;
    float frequency_hz = 2, damping_ratio = 0.7f;
    bool enable_limit = false;
    float min_length = 1, max_length = 100;
    bool enable_motor = false;
    float motor_speed = 0, max_motor_force = 0; // EU/s, kg*EU/s^2
    bool collide_connected = false;
};
struct RevoluteJointSettings
{
    bool enable_spring = false;
    float target_angle = 0, frequency_hz = 2, damping_ratio = 0.7f;
    bool enable_limit = false;
    float lower_angle = 0, upper_angle = 0;
    bool enable_motor = false;
    float motor_speed = 0, max_motor_torque = 0; // rad/s, kg*EU^2/s^2
    bool collide_connected = false;
};
struct DistanceJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{};
    DistanceJointSettings settings{};
};
struct RevoluteJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{};
    float reference_angle = 0;
    RevoluteJointSettings settings{};
};
struct DistanceJointState
{
    DistanceJointSettings settings{};
    float current_length = 0, speed = 0, motor_force = 0;
};
struct RevoluteJointState
{
    RevoluteJointSettings settings{};
    float angle = 0, angular_velocity = 0, motor_torque = 0;
};
struct WeldJointSettings
{
    float linear_frequency_hz = 0, angular_frequency_hz = 0;
    float linear_damping_ratio = 1, angular_damping_ratio = 1;
    bool collide_connected = false;
};
struct PrismaticJointSettings
{
    bool enable_spring = false;
    float target_translation = 0, frequency_hz = 2, damping_ratio = .7f;
    bool enable_limit = false;
    float lower_translation = 0, upper_translation = 100;
    bool enable_motor = false;
    float motor_speed = 0, max_motor_force = 0;
    bool collide_connected = false;
};
struct WheelJointSettings
{
    bool enable_spring = true;
    float frequency_hz = 2, damping_ratio = .7f;
    bool enable_limit = false;
    float lower_translation = 0, upper_translation = 100;
    bool enable_motor = false;
    float motor_speed = 0, max_motor_torque = 0;
    bool collide_connected = false;
};
struct MouseJointSettings
{
    elysia::core::Vector2 target{}; // world EU; initial target defines the drag anchor
    float frequency_hz = 4, damping_ratio = 1, max_force = 1000;
    bool collide_connected = false;
};
struct MotorJointSettings
{
    elysia::core::Vector2 linear_offset{}; // body A local EU
    float angular_offset = 0, max_force = 100, max_torque = 10000, correction_factor = .3f;
    bool collide_connected = false;
};
struct WeldJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{};
    float reference_angle = 0;
    WeldJointSettings settings{};
};
struct PrismaticJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{}, local_axis{1, 0};
    float reference_angle = 0;
    PrismaticJointSettings settings{};
};
struct WheelJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{}, local_axis{0, 1};
    WheelJointSettings settings{};
};
struct MouseJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    MouseJointSettings settings{};
};
struct MotorJointDefinition
{
    PhysicsObjectHandle first{}, second{};
    MotorJointSettings settings{};
};
struct FilterJointDefinition
{
    PhysicsObjectHandle first{}, second{};
};
struct WeldJointState
{
    WeldJointSettings settings{};
    elysia::core::Vector2 linear_error{};
    float angular_error = 0;
};
struct PrismaticJointState
{
    PrismaticJointSettings settings{};
    float translation = 0, speed = 0, motor_force = 0;
};
struct WheelJointState
{
    WheelJointSettings settings{};
    float translation = 0, speed = 0, angular_velocity = 0, motor_torque = 0;
};
struct MouseJointState
{
    MouseJointSettings settings{};
    elysia::core::Vector2 drag_point{}, error{};
};
struct MotorJointState
{
    MotorJointSettings settings{};
    elysia::core::Vector2 linear_error{};
    float angular_error = 0;
};
struct FilterJointState
{
    bool collision_disabled = true;
};
struct JointState
{
    elysia::core::Vector2 anchor_first{}, anchor_second{}, reaction_force{};
    float reaction_torque = 0;
    std::variant<DistanceJointState, RevoluteJointState, WeldJointState, PrismaticJointState, WheelJointState,
                 MouseJointState, MotorJointState, FilterJointState>
        details{};
    PhysicsObjectHandle first{}, second{};
};
} // namespace elysia::physics
