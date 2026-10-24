#pragma once

#include "settings_scene_payload.h"
#include "../../scene/scene.h"
#include "../../config/user_config_data.h"
#include "../../config/user_config_types.h"

namespace elysia::ui
{
class SettingsPanel;
struct SettingsPanelDraft;
class UiWindow;
}

namespace elysia::builtin
{
class SettingsScene final : public elysia::scene::Scene
{
public:
    SettingsScene() = default;
    ~SettingsScene() override = default;

protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:
    void build_ui();
    void restore_ui_state();
    void save_draft(const elysia::ui::SettingsPanelDraft& draft);
    void return_to_caller();
    void destroy_ui() noexcept;

private:
    elysia::scene::SceneRoute _return_route;
    elysia::config::UserConfigRuntimeState _baseline_state;
    elysia::ui::SettingsPanelVisibility _visibility;
    elysia::ui::UiWindow* _window = nullptr;
    elysia::ui::SettingsPanel* _settings_panel = nullptr;
    bool _transitioning = false;
};
}
