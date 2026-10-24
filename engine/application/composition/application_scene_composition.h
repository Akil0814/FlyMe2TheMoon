#pragma once

#include "../game_module.h"

namespace elysia::application
{
[[nodiscard]] ApplicationDescriptor describe_game_module(
    const IGameModule& game_module);

void compose_application_scenes(
    elysia::scene::SceneManager& scene_manager,
    const IGameModule& game_module,
    const ApplicationDescriptor& descriptor);
}
