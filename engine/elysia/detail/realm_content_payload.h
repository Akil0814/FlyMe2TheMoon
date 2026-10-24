#pragma once

#include "../../scene/routing/scene_route.h"

namespace elysia::realm::detail
{
struct RealmContentPayload
{
    elysia::scene::SceneRoute return_route{};
};
}
