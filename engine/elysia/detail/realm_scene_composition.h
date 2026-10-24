#pragma once

namespace elysia::scene
{
class SceneManager;
}

namespace elysia::realm::detail
{
void register_realm_scenes(
    elysia::scene::SceneManager& scene_manager);
}
