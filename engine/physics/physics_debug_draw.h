#pragma once

#include "physics_world_stats.h"

namespace elysia::tools { class DebugDraw; }

namespace elysia::physics
{
void submit_physics_debug_snapshot(
    const PhysicsDebugSnapshot& snapshot,
    elysia::tools::DebugDraw& debug_draw);
}
