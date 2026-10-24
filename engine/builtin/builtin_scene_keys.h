#pragma once

#include "../scene/routing/scene_key.h"

namespace elysia::builtin::SceneKeys
{
inline constexpr elysia::scene::SceneKey StartupLoading = 0xFFFF0001u;
inline constexpr elysia::scene::SceneKey Settings = 0xFFFF0002u;
inline constexpr elysia::scene::SceneKey ApplicationFailure = 0xFFFF0005u;
}
