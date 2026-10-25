#pragma once
#include "../tools/debug_draw.h"
#include "physics_world_stats.h"
namespace elysia::physics
{
inline constexpr auto kPhysicsDebugCategories =
    elysia::tools::DebugDrawCategory::PhysicsCollider | elysia::tools::DebugDrawCategory::PhysicsContact |
    elysia::tools::DebugDrawCategory::PhysicsContactNormal | elysia::tools::DebugDrawCategory::PhysicsBroadPhase |
    elysia::tools::DebugDrawCategory::PhysicsPoseHistory | elysia::tools::DebugDrawCategory::PhysicsVelocity |
    elysia::tools::DebugDrawCategory::PhysicsJoint | elysia::tools::DebugDrawCategory::PhysicsJointDetails |
    elysia::tools::DebugDrawCategory::PhysicsChainDetails | elysia::tools::DebugDrawCategory::PhysicsTarget;
[[nodiscard]] PhysicsDebugCapture physics_debug_capture_for(const elysia::tools::DebugDraw &) noexcept;
void submit_physics_debug_snapshot(const PhysicsDebugSnapshot &, elysia::tools::DebugDraw &);
} // namespace elysia::physics
