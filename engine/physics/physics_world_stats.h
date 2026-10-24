#pragma once

#include "collision/collision_contact.h"
#include "collision/world_shape.h"
#include "physics_object_handle.h"
#include "body/body_definition.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace elysia::physics
{
enum class PhysicsDebugCapture : std::uint8_t
{
    None = 0,
    Shapes = 1u << 0,
    BroadPhase = 1u << 1,
    Contacts = 1u << 2,
    Velocities = 1u << 3,
    Joints = 1u << 4,
    All = (1u << 5) - 1u
};

[[nodiscard]] constexpr PhysicsDebugCapture operator|(
    PhysicsDebugCapture first,
    PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(
        static_cast<std::uint8_t>(first) | static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr PhysicsDebugCapture operator&(
    PhysicsDebugCapture first,
    PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(
        static_cast<std::uint8_t>(first) & static_cast<std::uint8_t>(second));
}

constexpr PhysicsDebugCapture& operator|=(
    PhysicsDebugCapture& first,
    PhysicsDebugCapture second) noexcept
{
    first = first | second;
    return first;
}

[[nodiscard]] constexpr bool captures_physics_debug(
    PhysicsDebugCapture capture,
    PhysicsDebugCapture requested) noexcept
{
    return (capture & requested) != PhysicsDebugCapture::None;
}

struct PhysicsStepStats
{
    std::size_t registered_objects = 0;
    std::size_t registered_colliders = 0;
    std::size_t awake_bodies = 0;
    std::size_t joints = 0;
    double step_milliseconds = 0;
    std::size_t contacts = 0;
};

struct PhysicsDebugShape
{
    CollisionTarget target{};
    WorldColliderShape previous{WorldAabb{}};
    WorldColliderShape current{WorldAabb{}};
    elysia::core::Rect native_bounds{};
    PhysicsPose previous_pose{}, current_pose{};
    bool sensor = false;
    bool awake = true;
};

struct PhysicsDebugJoint
{
    elysia::core::Vector2 first_anchor{}, second_anchor{};
    PhysicsPose first_previous{}, first_current{}, second_previous{}, second_current{};
};

struct PhysicsDebugVelocity
{
    PhysicsObjectHandle object{};
    elysia::core::Vector2 origin{};
    elysia::core::Vector2 velocity{};
};

struct PhysicsDebugSnapshot
{
    std::vector<PhysicsDebugShape> shapes;
    std::vector<CollisionContact> contacts;
    std::vector<PhysicsDebugVelocity> velocities;
    std::vector<PhysicsDebugJoint> joints;
    float interpolation_alpha = 1;

    void clear() noexcept
    {
        shapes.clear();
        contacts.clear();
        velocities.clear();
        joints.clear();
    }
};
}
