#include "game/audio/game_audio.h"
#include "settings_scene.h"
#include "main_menu_scene.h"
#include "game/frontend/moon_ui.h"
#include "game/content/content.h"
#include "game/testing/replay_probe.h"
#include "engine/config/user_config_service.h"
#include "engine/audio/audio_service.h"
#include "engine/localization/localization_service.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_checkbox.h"
#include "engine/ui/widgets/ui_radio_button.h"
#include "engine/ui/widgets/ui_slider.h"

namespace game::scene {
using namespace elysia;
namespace {
void style_controls(ui::UiElement& element){
    if(auto* checkbox=dynamic_cast<ui::UiCheckbox*>(&element))checkbox->set_sounds({.toggle="fly.voice_6_3"});
    if(auto* radio=dynamic_cast<ui::UiRadioButton*>(&element))radio->set_sounds({.select="fly.voice_6_3"});
    if(auto* slider=dynamic_cast<ui::UiSlider*>(&element))slider->set_sounds({.on_settle="fly.voice_6_3"});
    if(auto* button=dynamic_cast<ui::UiButton*>(&element)){
        button->set_sounds({.click="fly.voice_6_3"});
        auto style=button->style_overrides();style.chrome.corner_radius=10.f;button->set_style_overrides(style);
    }
    if(auto* panel=dynamic_cast<ui::UiPanel*>(&element)){
        auto style=panel->style_overrides();style.background=core::Color{9,27,62,224};panel->set_style_overrides(style);
    }
    if(auto* host=dynamic_cast<ui::UiChildHost*>(&element))for(std::size_t index=0;index<host->child_count();++index)style_controls(*host->child_at(index));
}
ui::SettingsPanelDraft draft(const config::UserConfigData& data){
    return {.window_mode=data.window.mode==config::WindowMode::Windowed?ui::SettingsWindowMode::Windowed:ui::SettingsWindowMode::BorderlessFullscreen,
        .window_size={data.window.windowed_size.width,data.window.windowed_size.height},.target_fps=data.target_fps,.vsync=data.vsync,
        .master_volume=data.audio.master_volume,.music_volume=data.audio.music_volume,.sound_volume=data.audio.sound_volume,.language=data.language};
}
}
void SettingsScene::on_enter(const elysia::scene::ScenePayload&){
    ELYSIA_AUDIO->transition_music("fly.SkyTunnel");
    clear_scene_objects();_saved=false;_baseline=config::UserConfigService::instance()->user_config().runtime_state();
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720});
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    window->set_on_cancel([this]{game::audio::ui_click();request_scene_switch(scene_keys::MainMenu);});
    window->add_child(std::make_unique<frontend::MoonBackdrop>("settings",35));
    auto panel=std::make_unique<ui::SettingsPanel>(core::Rect{0,0,732,660},ui::SettingsPanelVisibility{.window_mode=false});_panel=panel.get();
    std::optional<ui::SettingsWindowSize> usable;
    if(auto* native=SDL_GetRenderWindow(runtime_context().renderer())){
        SDL_Rect bounds{};if(SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(native),&bounds))usable=ui::SettingsWindowSize{bounds.w,bounds.h};
    }
    _panel->set_options({.window_sizes=ui::make_settings_window_size_options(usable,draft(_baseline.settings).window_size),
        .target_fps_values=ui::make_settings_target_fps_options(_baseline.settings.target_fps),
        .languages=ELYSIA_LOCALIZATION->supported_languages(),.usable_window_size=usable});
    _panel->set_draft(draft(_baseline.settings));
    _panel->set_on_save([this](const ui::SettingsPanelDraft& value){save(value);});
    _panel->set_on_back([this]{request_scene_switch(scene_keys::MainMenu);});
    window->add_child(std::move(panel),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::CenterRight,._margin={.right=28}});
    _panel->register_with_window(*window);style_controls(*_panel);window->register_focus_scope(*_panel);window->focus_first_available_scope();
    if(_baseline.restart_required)_panel->set_status_content(ui::ui_text_key("engine.settings.status.restart_required"),false);
}
void SettingsScene::save(const ui::SettingsPanelDraft& value){
    auto* service=config::UserConfigService::instance();auto requested=service->user_config().snapshot();
    requested.window={value.window_mode==ui::SettingsWindowMode::Windowed?config::WindowMode::Windowed:config::WindowMode::BorderlessFullscreen,
        {value.window_size.width,value.window_size.height}};
    requested.target_fps=value.target_fps;requested.vsync=value.vsync;
    requested.audio={value.master_volume,value.music_volume,value.sound_volume};requested.language=value.language;
    const auto result=service->apply_and_save_user_config(requested,_baseline);
    _saved=result.has_value();
    _baseline=service->user_config().runtime_state();_panel->set_draft(draft(_baseline.settings));
    _panel->set_status_content(ui::ui_text_key(!result?"engine.settings.status.save_failed":_baseline.restart_required?"engine.settings.status.saved_restart_required":"engine.settings.status.saved"),!result);
}
void SettingsScene::on_exit(){if(_panel)_panel->unregister_from_window();_panel=nullptr;clear_scene_objects();}
void SettingsScene::on_reset(){on_exit();}
void SettingsScene::on_after_update(double){if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",6},{"settings_ready",_panel!=nullptr},{"settings_saved",_saved},{"master_volume",_baseline.settings.audio.master_volume}}))request_quit();}
}
