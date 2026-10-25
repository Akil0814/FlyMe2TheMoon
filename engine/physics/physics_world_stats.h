#pragma once

#include "body/body_definition.h"
#include "chain_definition.h"
#include "collision/collision_contact.h"
#include "collision/world_shape.h"
#include "joint_definition.h"
#include "physics_object_handle.h"
#include <utility>

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
    Chains = 1u << 5,
    Targets = 1u << 6,
    All = (1u << 7) - 1u
};

[[nodiscard]] constexpr PhysicsDebugCapture operator|(PhysicsDebugCapture first, PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(static_cast<std::uint8_t>(first) | static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr PhysicsDebugCapture operator&(PhysicsDebugCapture first, PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(static_cast<std::uint8_t>(first) & static_cast<std::uint8_t>(second));
}

constexpr PhysicsDebugCapture &operator|=(PhysicsDebugCapture &first, PhysicsDebugCapture second) noexcept
{
    first = first | second;
    return first;
}

[[nodiscard]] constexpr bool captures_physics_debug(PhysicsDebugCapture capture, PhysicsDebugCapture requested) noexcept
{
    return (capture & requested) != PhysicsDebugCapture::None;
}

struct PhysicsStepStats
{
    std::size_t registered_objects = 0;
    std::size_t registered_colliders = 0;
    std::size_t awake_bodies = 0;
    std::size_t joints = 0;
    std::size_t chains = 0;
    double step_milliseconds = 0;
    std::size_t contacts = 0;
};

struct PhysicsDebugShape
{
    CollisionTarget target{};
    WorldColliderShape previous{WorldPolygon{}};
    WorldColliderShape current{WorldPolygon{}};
    elysia::core::Rect native_bounds{};
    PhysicsPose previous_pose{}, current_pose{};
    bool sensor = false;
    bool awake = true;
};

struct PhysicsDebugJoint
{
    JointHandle handle{};
    JointState state{};
    PhysicsPose first_previous{}, first_current{}, second_previous{}, second_current{};
    elysia::core::Vector2 local_anchor_first{}, local_anchor_second{}, local_axis_first{1, 0};
    float reference_angle = 0;
};
struct PhysicsDebugChain
{
    ChainHandle handle{};
    PhysicsObjectHandle owner{};
    bool loop = false;
    std::vector<elysia::core::Vector2> local_vertices = std::vector<elysia::core::Vector2>(std::size_t{0});
    std::vector<ColliderId> colliders = std::vector<ColliderId>(std::size_t{0});
    std::optional<elysia::core::Vector2> ghost_before{}, ghost_after{};
    PhysicsPose previous{}, current{};
    PhysicsDebugChain() = default;
    // Copy on vector growth: MSVC Debug vector moves allocate in noexcept code.
    PhysicsDebugChain(const PhysicsDebugChain &) = default;
    PhysicsDebugChain &operator=(const PhysicsDebugChain &) = default;
};
struct PhysicsDebugTarget
{
    PhysicsObjectHandle owner{};
    PhysicsPose pose_at_command{}, requested_pose{}, effective_pose{};
    PhysicsPose previous{}, current{};
    double fixed_delta_seconds = 0;
    bool pending = false;
    std::vector<WorldColliderShape> shapes = std::vector<WorldColliderShape>(std::size_t{0});
    PhysicsDebugTarget() = default;
    PhysicsDebugTarget(const PhysicsDebugTarget &) = default;
    PhysicsDebugTarget &operator=(const PhysicsDebugTarget &) = default;
};
struct PhysicsDebugVelocity
{
    PhysicsObjectHandle object{};
    elysia::core::Vector2 origin{};
    elysia::core::Vector2 velocity{};
};

struct PhysicsDebugSnapshot
{
    struct Body
    {
        PhysicsObjectHandle owner{};
        MassProperties mass_properties{};
        BodyState state{};
    };
    std::vector<Body> bodies = std::vector<Body>(std::size_t{0});
    std::vector<PhysicsDebugShape> shapes = std::vector<PhysicsDebugShape>(std::size_t{0});
    std::vector<CollisionContact> contacts = std::vector<CollisionContact>(std::size_t{0});
    std::vector<PhysicsDebugVelocity> velocities = std::vector<PhysicsDebugVelocity>(std::size_t{0});
    std::vector<PhysicsDebugJoint> joints = std::vector<PhysicsDebugJoint>(std::size_t{0});
    std::vector<PhysicsDebugChain> chains = std::vector<PhysicsDebugChain>(std::size_t{0});
    std::vector<PhysicsDebugTarget> targets = std::vector<PhysicsDebugTarget>(std::size_t{0});
    float interpolation_alpha = 1, units_per_meter = 100;
    PhysicsDebugSnapshot() = default;
    PhysicsDebugSnapshot(const PhysicsDebugSnapshot &) = default;
    PhysicsDebugSnapshot &operator=(const PhysicsDebugSnapshot &) = default;
    PhysicsDebugSnapshot(PhysicsDebugSnapshot &&other) : PhysicsDebugSnapshot()
    {
        swap(*this, other);
    }
    PhysicsDebugSnapshot &operator=(PhysicsDebugSnapshot &&other) noexcept
    {
        swap(*this, other);
        return *this;
    }
    friend void swap(PhysicsDebugSnapshot &a, PhysicsDebugSnapshot &b) noexcept
    {
        a.bodies.swap(b.bodies);
        a.shapes.swap(b.shapes);
        a.contacts.swap(b.contacts);
        a.velocities.swap(b.velocities);
        a.joints.swap(b.joints);
        a.chains.swap(b.chains);
        a.targets.swap(b.targets);
        std::swap(a.interpolation_alpha, b.interpolation_alpha);
        std::swap(a.units_per_meter, b.units_per_meter);
    }
    void clear() noexcept
    {
        bodies.clear();
        shapes.clear();
        contacts.clear();
        velocities.clear();
        joints.clear();
        chains.clear();
        targets.clear();
    }
};
} // namespace elysia::physics
