#include "main_menu_scene.h"
#include "flight_scene.h"
#include "credits_scene.h"
#include "selection_scene.h"
#include "settings_scene.h"
#include "game/frontend/moon_ui.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene_payload.h"
#include "engine/ui/widgets/image/ui_image.h"
#include "engine/resources/resource_service.h"
#include "engine/audio/audio_service.h"
#include "engine/tools/logger.h"

#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/containers/ui_panel.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace game::scene
{
void MainMenuScene::on_enter(const elysia::scene::ScenePayload&)
{
    clear_scene_objects();
    testing::ReplayProbe::shared().begin();

    const auto& context = runtime_context();
    auto* window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0, 0,
            static_cast<float>(context.logical_width()),
            static_cast<float>(context.logical_height())});
    _window=window;
    window->set_on_cancel([this] { open_exit_dialog(); });
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    window->add_child(std::make_unique<frontend::MoonBackdrop>("menu"));
    // Menu scenes share this track; transitioning to the same key preserves playback.
    ELYSIA_AUDIO->transition_music("fly.MoonTrip");

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{0, 0, 316, 336});
    content->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    content->set_item_spacing(14);

    auto play = std::make_unique<frontend::GlassButton>(game::content::ui("play"),300,58,[this] {request_scene_switch(game::scene_keys::Selection);});
    content->add_back(std::move(play));
    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("achievements"),300,58,[this]{request_scene_switch(game::scene_keys::Achievements);}));
    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("settings"),300,58,[this]{request_scene_switch(game::scene_keys::Settings);}));

    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("credits"),300,58,[this]{request_scene_switch(game::scene_keys::Credits);}));

    auto exit = std::make_unique<frontend::GlassButton>(game::content::ui("exit"),300,48,[this] { open_exit_dialog(); });
    content->add_back(std::move(exit));

    auto* focus_scope = content.get();
    window->add_child(std::move(content),
        elysia::ui::UiLayoutChildOptions{._anchor = elysia::ui::UiLayoutAnchor::CenterRight,._margin={.right=48}});
    window->register_focus_scope(*focus_scope);
    window->focus_first_available_scope();

    auto dialog=std::make_unique<elysia::ui::UiPanel>(elysia::core::Rect{0,0,1280,720});
    _exit_dialog=dialog.get();
    dialog->set_style_overrides({.draw_background=true,.draw_border=false,.background=elysia::core::Color{2,8,24,170}});
    dialog->add_child(std::make_unique<frontend::HudPanel>(elysia::core::Rect{0,0,640,340},-1),
        elysia::ui::UiLayoutChildOptions{._anchor=elysia::ui::UiLayoutAnchor::Center});
    auto actions=std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{0,0,560,256});
    actions->set_cross_align(elysia::ui::UiLayoutAlign::Center);actions->set_item_spacing(16);
    auto title=frontend::label(game::content::ui("exit_confirm_title"),560,48,true);
    title->set_typography_role(elysia::typography::UiTypographyRole::Heading);
    title->set_style_overrides({.text=elysia::core::Color{16,46,77,255}});
    title->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
    actions->add_back(std::move(title));
    auto message=frontend::label(game::content::ui("exit_confirm_message"),560,64,true);
    message->set_style_overrides({.text=elysia::core::Color{16,46,77,255}});
    message->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
    actions->add_back(std::move(message));
    actions->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("exit_cancel"),360,48,[this]{close_exit_dialog();}));
    actions->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("exit_confirm"),360,48,[this]{
        ELYSIA_LOG_INFO("fly.menu","Exit confirmed");request_quit();}));
    dialog->add_child(std::move(actions),elysia::ui::UiLayoutChildOptions{._anchor=elysia::ui::UiLayoutAnchor::Center});
    window->add_child(std::move(dialog));
    if(!window->register_overlay(*_exit_dialog,{.open=false,.modal=true,.close_on_cancel=true,.close_on_outside_click=false}))
        throw std::runtime_error("Could not register exit confirmation overlay");
}

void MainMenuScene::open_exit_dialog(){
    if(!_window||!_exit_dialog||_window->is_overlay_open(*_exit_dialog))return;
    _window->open_overlay(*_exit_dialog);
    _exit_dialog->focus_first_available();
}
void MainMenuScene::close_exit_dialog(){
    if(_window&&_exit_dialog)_window->close_overlay(*_exit_dialog);
}
void MainMenuScene::on_exit()
{
    _window=nullptr;_exit_dialog=nullptr;
    clear_scene_objects();
}
void MainMenuScene::on_after_update(double) {
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",1},
        {"exit_confirm_open",_window&&_exit_dialog&&_window->is_overlay_open(*_exit_dialog)}}))request_quit();
}

void MainMenuScene::on_reset()
{
    on_exit();
}
}
