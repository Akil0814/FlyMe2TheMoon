#pragma once

#include "scene_route.h"

namespace elysia::scene
{
enum class SceneRequestType
{
    None,
    Switch,
    Quit
};

struct SceneRequest
{
    SceneRequestType type = SceneRequestType::None;
    SceneRoute route{};
};

}
