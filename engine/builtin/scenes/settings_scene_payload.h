#pragma once

#include "../../scene/routing/scene_route.h"
#include "../../ui/presets/settings_panel.h"

namespace elysia::builtin
{
struct SettingsScenePayload
{
    elysia::scene::SceneRoute return_route{};
    elysia::ui::SettingsPanelVisibility visibility{};
};
}
