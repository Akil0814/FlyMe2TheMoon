#include "main_menu_scene.h"
#include "flight_scene.h"
#include "selection_scene.h"
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
    window->set_style_overrides({.draw_border=false,.background=elysia::core::Color{5,13,32,255}});
    window->add_child(std::make_unique<elysia::ui::UiImage>(ELYSIA_RESOURCES->find_texture("fly.ui.mainBg2"),elysia::core::Rect{0,0,480,720}),
        elysia::ui::UiLayoutChildOptions{._anchor=elysia::ui::UiLayoutAnchor::CenterLeft});
    ELYSIA_AUDIO->play_music("fly.MoonTrip");

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{0, 0, 600, 600});
    content->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    content->set_item_spacing(22);

    auto title = std::make_unique<elysia::ui::UiImage>(
        ELYSIA_RESOURCES->find_texture("fly.ui.mainTitle2"),elysia::core::Rect{0,0,480,207});
    content->add_back(std::move(title));

    auto play = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{0,0,360,56});
    play->set_text_content(elysia::ui::ui_raw_text(game::content::ui("play")));
    play->set_on_click([this] {request_scene_switch(game::scene_keys::Selection);});
    content->add_back(std::move(play));
    auto achievements=std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{0,0,360,56});
    achievements->set_text_content(elysia::ui::ui_raw_text(game::content::ui("achievements")));achievements->set_on_click([this]{request_scene_switch(game::scene_keys::Achievements);});content->add_back(std::move(achievements));
    auto settings=std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{0,0,360,56});
    settings->set_text_content(elysia::ui::ui_raw_text(game::content::ui("settings")));settings->set_on_click([this]{request_scene_switch(elysia::builtin::SceneKeys::Settings,elysia::builtin::SettingsScenePayload{.return_route={.target=game::scene_keys::MainMenu}});});content->add_back(std::move(settings));

    auto exit = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{0, 0, 360, 56});
    exit->set_text_content(elysia::ui::ui_raw_text(game::content::ui("exit")));
    exit->set_on_click([this] { request_quit(); });
    content->add_back(std::move(exit));

    auto* focus_scope = content.get();
    window->add_child(std::move(content),
        elysia::ui::UiLayoutChildOptions{._anchor = elysia::ui::UiLayoutAnchor::CenterRight,._margin={.right=70}});
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
