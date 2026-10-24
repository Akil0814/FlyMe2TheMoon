#pragma once

#include "../../scene/scene.h"
#include "../../tools/timer.h"
#include "../../ui/style/ui_theme_manager.h"

#include "../elysia_realm.h"

#include <cstddef>
#include <string_view>

namespace elysia::ui
{
class UiWindow;
class UiListContainer;
}

namespace elysia::realm::detail
{
class ElysiaIntroScene final : public elysia::scene::Scene
{
public:
    ElysiaIntroScene() = default;
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_before_update(double delta) override;
    void on_after_update(double delta) override;
    void on_exit() override;
    void on_reset() override;

private:
    void reveal_next_code_line();
    void add_label(std::string_view code_line);
    void request_realm_transition();
    void stop_playback() noexcept;
    void build_ui();
    void destroy_ui() noexcept;

private:
    std::size_t _current_line = 0;
    bool _logo_finished = false;
    bool _code_finished = false;
    bool _transition_requested = false;
    bool _music_handed_off = false;

    elysia::tools::Timer _code_timer;

    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    elysia::ui::UiListContainer* _code_list = nullptr;
    elysia::ui::UiThemeManager _elysia_theme;
    elysia::ui::UiThemeRegistration _theme_registration;
};
}
