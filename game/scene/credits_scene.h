#pragma once
#include "engine/scene/scene.h"
#include "engine/ui/containers/ui_scroll_container.h"
namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Credits=5;}
namespace game::scene {
class CreditsScene final : public elysia::scene::Scene {
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override {on_exit();}
    void on_after_update(double) override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
    elysia::ui::UiScrollContainer* _scroll=nullptr;
    bool _auto_scroll=true;
    double _wait=2;
};
}
