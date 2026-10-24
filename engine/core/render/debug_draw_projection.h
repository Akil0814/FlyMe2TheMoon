#pragma once

#include "render_command.h"

#include "../../camera/camera.h"
#include "../../tools/debug_draw.h"

#include <span>
#include <vector>

namespace elysia::core
{
void append_projected_debug_draw_commands(
    std::span<const elysia::tools::DebugDrawCommand> debug_commands,
    elysia::tools::DebugDrawCategory enabled_categories,
    const elysia::camera::Camera& camera,
    std::vector<UiRenderCommand>& out_commands
);
}
