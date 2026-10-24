#pragma once

#include "../../scene/scene.h"
#include "../../ui/style/ui_theme_manager.h"

#include "realm_content_payload.h"

namespace elysia::ui
{
class UiWindow;
}

namespace elysia::realm::detail
{
class ElysiaRealmScene final : public elysia::scene::Scene
{
public:
    ElysiaRealmScene() = default;
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:
    void build_ui();
    void destroy_ui() noexcept;
    void return_to_caller();

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    elysia::ui::UiThemeManager _elysia_theme;
    elysia::ui::UiThemeRegistration _theme_registration;
};
}
