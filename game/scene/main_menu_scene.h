#pragma once

#include "../../engine/scene/scene.h"

namespace elysia::ui {class UiWindow;class UiPanel;}
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
    void on_after_update(double)override;
private:
    void open_exit_dialog();
    void close_exit_dialog();
    elysia::ui::UiWindow* _window=nullptr;
    elysia::ui::UiPanel* _exit_dialog=nullptr;
};
}
