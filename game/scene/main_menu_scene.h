#pragma once

#include "../../engine/scene/scene.h"

namespace game::scene_keys
{
inline constexpr elysia::scene::SceneKey MainMenu = 1;
}

namespace game::scene
{
class MainMenuScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
};
}
