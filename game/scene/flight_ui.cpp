#include "flight_scene.h"
#include "engine/resources/resource_service.h"
#include "engine/ui/widgets/image/ui_image.h"

namespace game::scene {
using namespace elysia;
void FlightScene::build_ingame_ui(){
    _ui_layout=frontend::FlightUiLayout::make(runtime_context().logical_width(),runtime_context().logical_height());
    _hud_stars=create_and_add_object<frontend::StarRating>(_ui_layout.stars,11);
    _star_pickups=create_and_add_object<frontend::StarPickupEffects>();_star_pickups->set_screen_rect(_ui_layout.viewport);
    _hud=create_and_add_object<frontend::HudLabel>(_ui_layout.stage,11);
    _hud_stats=create_and_add_object<frontend::RunStats>(_ui_layout.stats,11);
    if(_ui_layout.vertical){_hud->set_padding(4);_hud_stats->set_grouped(true);}
    _pause_button=create_and_add_object<frontend::PauseIconButton>(_ui_layout.pause,[this]{open_pause_menu();});
    _dialogue_panel=create_and_add_object<frontend::HudPanel>(_ui_layout.dialogue,20);
    _portrait_panel=create_and_add_object<frontend::HudPanel>(_ui_layout.portrait,21);
    _face=create_and_add_object<ui::UiImage>(nullptr,_ui_layout.face,22);
    _dialogue=create_and_add_object<frontend::StoryText>(_ui_layout.text,22);_dialogue->set_padding(8);
    _dialogue->set_style_overrides({.text=core::Color{16,46,77,255}});
    _tutorial=create_and_add_object<frontend::TutorialUi>(_ui_layout.viewport);
}
void FlightScene::update_hud_visibility(){
    const bool visible=!_pause_menu_open&&!_finished&&!_demo&&_tutorial->type()==0;
    _hud->set_visible(visible);_hud_stars->set_visible(visible);_hud_stats->set_visible(visible);_star_pickups->set_visible(visible);
    _pause_button->set_visible(!_finished);
}
}
