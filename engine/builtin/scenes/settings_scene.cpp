#include "settings_scene.h"

#include "../../config/user_config_service.h"
#include "../../localization/localization_service.h"
#include "../../tools/logger.h"
#include "../../ui/layout/ui_layout_types.h"
#include "../../ui/presets/settings_panel.h"
#include "../../ui/window/ui_window.h"
#include "../../scene/runtime/scene_runtime_context.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace elysia::builtin
{
using elysia::scene::Scene;
using elysia::scene::ScenePayload;
using elysia::scene::SceneRoute;
using elysia::scene::try_scene_payload;

namespace
{
bool is_valid_route(const SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

elysia::ui::SettingsPanelDraft make_draft(const elysia::config::UserConfigData& settings)
{
    return elysia::ui::SettingsPanelDraft{ .window_mode =
            settings.window.mode == elysia::config::WindowMode::Windowed
            ? elysia::ui::SettingsWindowMode::Windowed
            : elysia::ui::SettingsWindowMode::BorderlessFullscreen,
        .window_size = { settings.window.windowed_size.width,settings.window.windowed_size.height},
        .target_fps = settings.target_fps,
        .vsync = settings.vsync,
        .master_volume = settings.audio.master_volume,
        .music_volume = settings.audio.music_volume,
        .sound_volume = settings.audio.sound_volume,
        .language = settings.language
    };
}

elysia::ui::SettingsPanelOptions make_panel_options(
    const elysia::config::UserConfigData& settings, SDL_Renderer* renderer)
{
    SDL_Window* window = renderer ? SDL_GetRenderWindow(renderer) : nullptr;
    SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();
    if (!display)
        display = SDL_GetPrimaryDisplay();
    SDL_Rect usable_bounds{};
    std::optional<elysia::ui::SettingsWindowSize> usable_size;
    if (display && SDL_GetDisplayUsableBounds(display,&usable_bounds))
    {
        usable_size = {
            usable_bounds.w,
            usable_bounds.h
        };
    }
    auto window_sizes = elysia::ui::make_settings_window_size_options(
        usable_size,
        {
            settings.window.windowed_size.width,
            settings.window.windowed_size.height
        });

    const auto& supported_languages =
        ELYSIA_LOCALIZATION->supported_languages();
    return elysia::ui::SettingsPanelOptions{
        .window_sizes = std::move(window_sizes),
        .target_fps_values =
            elysia::ui::make_settings_target_fps_options(
                settings.target_fps),
        .languages = supported_languages,
        .usable_window_size = usable_size
    };
}

void log_failure(const elysia::config::UserConfigCommitFailure& failure)
{
    const auto diagnostic = elysia::config::to_failure_diagnostic(failure);
    const auto report = elysia::core::format_failure_diagnostic(
        diagnostic,"SETTINGS-APPLY","settings");
    elysia::tools::Logger::instance()->log_stream(
        elysia::tools::LogLevel::Error,"settings",[&](std::ostream& output) {
            output << report;
        },diagnostic.origin);
}
}

void SettingsScene::on_enter(const ScenePayload& payload)
{
    const SettingsScenePayload* settings_payload =try_scene_payload<SettingsScenePayload>(payload);
    if (!settings_payload)
        throw std::logic_error("SettingsScene requires SettingsScenePayload.");
    if (!is_valid_route(settings_payload->return_route))
        throw std::logic_error("SettingsScene requires a valid return route.");

    auto* config_service = elysia::config::UserConfigService::instance();

    if (!config_service->is_initialized())
        throw std::logic_error("SettingsScene requires an initialized UserConfigService.");

    _return_route = settings_payload->return_route;
    _baseline_state = config_service->user_config().runtime_state();
    _transitioning = false;
    _paused = false;

    const bool visibility_changed =
        _visibility != settings_payload->visibility;
    if (visibility_changed && _window && !_window->is_destroyed())
        destroy_ui();
    _visibility = settings_payload->visibility;

    if (!_window || _window->is_destroyed())
        build_ui();
    restore_ui_state();
}

void SettingsScene::on_exit()
{
    _paused = false;
    _transitioning = false;

    if (_settings_panel)
        _settings_panel->unregister_from_window();

    if (_window && !_window->is_destroyed())
    {
        _window->set_active(false);
        _window->set_visible(false);
    }
}

void SettingsScene::on_reset()
{
    _paused = false;
    _transitioning = false;
    destroy_ui();
    _return_route = {};
    _baseline_state = {};
    _visibility = {};
}

void SettingsScene::build_ui()
{
    const int logical_width = runtime_context().logical_width();
    const int logical_height = runtime_context().logical_height();

    _window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0.0f,0.0f,static_cast<float>(logical_width),static_cast<float>(logical_height)},10);

    if (!_window)
        throw std::runtime_error("SettingsScene could not create its UiWindow.");

    const float panel_width = std::min(732.0f,static_cast<float>(logical_width) - 32.0f);
    const float panel_height = std::min(680.0f,static_cast<float>(logical_height) - 32.0f);

    auto panel = std::make_unique<elysia::ui::SettingsPanel>(
        elysia::core::Rect{ 0,0,panel_width,panel_height },
        _visibility);

    _settings_panel = panel.get();
    _settings_panel->set_on_save([this](const elysia::ui::SettingsPanelDraft& draft){save_draft(draft);});
    _settings_panel->set_on_back([this]() { return_to_caller(); });

    elysia::ui::UiElement* adopted = _window->add_child(std::move(panel),{ elysia::ui::UiLayoutAnchor::Center });

    if (!adopted)
        throw std::runtime_error("SettingsScene could not adopt its SettingsPanel.");

    _window->register_focus_scope(*_settings_panel);
    _window->set_on_cancel([this]() { return_to_caller(); });
}

void SettingsScene::restore_ui_state()
{
    if (!_window || !_settings_panel || _window->is_destroyed())
        return;

    _settings_panel->set_draft(make_draft(_baseline_state.settings));
    _settings_panel->set_options(make_panel_options(
        _baseline_state.settings, runtime_context().renderer()));
    _settings_panel->reset_navigation_state();
    if (_baseline_state.restart_required)
    {
        _settings_panel->set_status_content(
            elysia::ui::ui_text_key(
                "engine.settings.status.restart_required"),false);
    }
    else
    {
        _settings_panel->clear_status_message();
    }
    _settings_panel->register_with_window(*_window);

    _window->set_visible(true);
    _window->set_active(true);
    _window->focus_first_available_scope();
}

void SettingsScene::save_draft(const elysia::ui::SettingsPanelDraft& draft)
{
    if (_transitioning || !_settings_panel)
        return;

    auto* config_service = elysia::config::UserConfigService::instance();
    elysia::config::UserConfigData requested =config_service->user_config().snapshot();

    requested.window = {draft.window_mode == elysia::ui::SettingsWindowMode::Windowed
            ? elysia::config::WindowMode::Windowed: elysia::config::WindowMode::BorderlessFullscreen,
        {
            draft.window_size.width,
            draft.window_size.height
        }
    };
    requested.target_fps = draft.target_fps;
    requested.vsync = draft.vsync;
    requested.audio.master_volume = draft.master_volume;
    requested.audio.music_volume = draft.music_volume;
    requested.audio.sound_volume = draft.sound_volume;
    requested.language = draft.language;

    const auto result = config_service->apply_and_save_user_config(requested,_baseline_state);

    if (result)
    {
        _baseline_state = config_service->user_config().runtime_state();
        _settings_panel->set_draft(make_draft(_baseline_state.settings));
        _settings_panel->set_status_content(
            elysia::ui::ui_text_key(
                _baseline_state.restart_required
                    ? "engine.settings.status.saved_restart_required"
                    : "engine.settings.status.saved"),false);
        return;
    }

    log_failure(result.error());

    // The service snapshot is authoritative after either a complete rollback
    // or a partial rollback failure.
    _baseline_state = config_service->user_config().runtime_state();
    _settings_panel->set_draft(make_draft(_baseline_state.settings));
    _settings_panel->set_status_content(
        elysia::ui::ui_text_key("engine.settings.status.save_failed"),true);
}

void SettingsScene::return_to_caller()
{
    if (_transitioning)
        return;

    _transitioning = true;

    if (_settings_panel)
        _settings_panel->set_draft(make_draft(_baseline_state.settings));

    request_scene_switch(_return_route);
}

void SettingsScene::destroy_ui() noexcept
{
    if (_settings_panel)
        _settings_panel->unregister_from_window();
    if (_window)
        _window->destroy();
    _settings_panel = nullptr;
    _window = nullptr;
}
}
