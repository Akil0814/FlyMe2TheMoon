#pragma once

#include "../../physics/physics_world_config.h"
#include "scene_camera_config.h"

#include <cstdint>
#include <optional>

namespace elysia::scene
{
struct FixedStepConfig
{
    double delta_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_frame = 8;
};

struct SceneRuntimeFeatures
{
    std::optional<FixedStepConfig> fixed_step;
    std::optional<elysia::physics::PhysicsWorldConfig> physics;
    std::optional<CameraSceneConfig> camera;
};
} // namespace elysia::scene
