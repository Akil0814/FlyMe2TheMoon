#pragma once

#include "scene_key.h"
#include "scene_payload.h"

namespace elysia::scene
{
enum class SceneReloadMode
{
    Reuse,
    Reset,
    Recreate
};

struct SceneRoute
{
    SceneKey target = SceneKeys::Invalid;
    ScenePayload payload{};
    SceneReloadMode reload_mode = SceneReloadMode::Reuse;
};
}
