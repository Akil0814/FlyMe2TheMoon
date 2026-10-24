#pragma once

#include "../../scene/routing/scene_route.h"

#include <optional>
#include <string>

namespace elysia::builtin
{
struct StartupLogoSlot
{
    std::string texture_key;
    double fade_in_seconds = 1.0;
    double hold_seconds = 1.0;
    double fade_out_seconds = 1.0;
};

struct StartupLoadingScenePayload
{
    elysia::scene::SceneRoute success_route{};
    std::optional<elysia::scene::SceneRoute> failure_route{};
    std::optional<StartupLogoSlot> project_logo{};
    bool wait_for_logo_sequence = true;
    bool wait_for_confirmation = true;
};
}
