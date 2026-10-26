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

#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/window/ui_window.h"

#include <memory>

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
    window->set_on_cancel([this] { request_quit(); });
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    window->add_child(std::make_unique<frontend::MoonBackdrop>("menu_outpaint"));
    window->add_child(std::make_unique<frontend::MenuArtwork>(),elysia::ui::UiLayoutChildOptions{._anchor=elysia::ui::UiLayoutAnchor::TopCenter});
    ELYSIA_AUDIO->play_music("fly.MoonTrip");

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{0, 0, 316, 336});
    content->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    content->set_item_spacing(14);

    auto play = std::make_unique<frontend::GlassButton>(game::content::ui("play"),300,58,[this] {request_scene_switch(game::scene_keys::Selection);});
    content->add_back(std::move(play));
    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("achievements"),300,58,[this]{request_scene_switch(game::scene_keys::Achievements);}));
    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("settings"),300,58,[this]{request_scene_switch(game::scene_keys::Settings);}));

    content->add_back(std::make_unique<frontend::GlassButton>(game::content::ui("credits"),300,58,[this]{request_scene_switch(game::scene_keys::Credits);}));

    auto exit = std::make_unique<frontend::GlassButton>(game::content::ui("exit"),300,48,[this] { request_quit(); });
    content->add_back(std::move(exit));

    auto* focus_scope = content.get();
    window->add_child(std::move(content),
        elysia::ui::UiLayoutChildOptions{._anchor = elysia::ui::UiLayoutAnchor::CenterRight,._margin={.right=48}});
    window->register_focus_scope(*focus_scope);
    window->focus_first_available_scope();
}

void MainMenuScene::on_exit()
{
    ELYSIA_AUDIO->stop_music();
    clear_scene_objects();
}
void MainMenuScene::on_after_update(double) {
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",1}}))request_quit();
}

void MainMenuScene::on_reset()
{
    clear_scene_objects();
}
}
