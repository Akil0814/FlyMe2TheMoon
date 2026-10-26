#include "flight_scene.h"
#include "selection_scene.h"
#include "credits_scene.h"
#include "game/progress/progress.h"
#include "game/progress/scoring.h"
#include "engine/ui/containers/ui_list_container.h"

namespace game::scene {
using namespace elysia;
namespace {
std::unique_ptr<ui::UiListContainer> modal_list(){
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,600,440});
    list->set_cross_align(ui::UiLayoutAlign::Center);list->set_item_spacing(12);return list;
}
ui::UiLabel* add_label(ui::UiListContainer& list,float height,bool title=false){
    auto label=frontend::label("",600,height,true);auto* ptr=label.get();
    if(title)ptr->set_typography_role(typography::UiTypographyRole::Heading);
    list.add_back(std::move(label));return ptr;
}
ui::UiButton* add_button(ui::UiListContainer& list,const char* key,std::function<void()> action){
    auto button=std::make_unique<frontend::GlassButton>(content::ui(key),440,48,std::move(action));auto* ptr=button.get();
    list.add_back(std::move(button));return ptr;
}
frontend::StarRating* add_stars(ui::UiListContainer& list){
    auto stars=std::make_unique<frontend::StarRating>(core::Rect{0,0,216,64});auto* ptr=stars.get();list.add_back(std::move(stars));return ptr;
}
void adopt_list(ui::UiWindow& window,std::unique_ptr<ui::UiListContainer> list){
    auto* scope=list.get();window.add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    window.register_focus_scope(*scope);window.set_visible(false);window.set_active(false);
}
}
ui::UiWindow* FlightScene::build_modal_frame(){
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720},30);
    window->set_style_overrides({.draw_border=false,.background=core::Color{2,8,24,170}});
    window->add_child(std::make_unique<frontend::MoonPanel>(core::Rect{0,0,680,500},-1),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    return window;
}
void FlightScene::build_result_panel(){
    _result=build_modal_frame();
    // Leaving a completed level is an explicit button action.
    _result->set_on_cancel([this]{_result->focus_first_available_scope();});
    auto list=modal_list();
    _result_title=add_label(*list,48,true);_result_stars=add_stars(*list);
    _result_stats=add_label(*list,40);_result_save=add_label(*list,32);
    _result_continue=add_button(*list,"next_level",[this]{continue_from_result();});
    add_button(*list,"retry",[this]{retry_level();});
    add_button(*list,"select_level",[this]{request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});});
    adopt_list(*_result,std::move(list));
}
void FlightScene::build_pause_panel(){
    _pause_menu=build_modal_frame();_pause_menu->set_on_cancel([this]{close_pause_menu();});
    auto list=modal_list();add_label(*list,48,true)->set_text_content(ui::ui_raw_text(content::ui("pause_title")));
    _pause_stars=add_stars(*list);_pause_stats=add_label(*list,40);add_label(*list,32);
    add_button(*list,"resume_level",[this]{close_pause_menu();});
    add_button(*list,"retry",[this]{retry_level();});
    add_button(*list,"select_level",[this]{request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});});
    adopt_list(*_pause_menu,std::move(list));
}
void FlightScene::open_pause_menu(){
    if(_pause_menu_open||_finished||_event_pending||!_player)return;
    _pause_was_logic_paused=is_paused();_pause_menu_open=true;
    _pause_updates.clear();
    const auto freeze=[this](core::GameObject* object){if(object->update_when_paused()){_pause_updates.push_back(object);object->set_update_when_paused(false);}};
    freeze(_player);for(auto* entity:_entities)freeze(entity);
    _player->thrust={};pause();set_all_gameplay_input_blocked(true);
    _pause_stars->set_collected(_stars);
    _pause_stats->set_text_content(ui::ui_raw_text(content::format("pause_stats",_level,_crystals,_elapsed)));
    _pause_button->set_active(false);_hint->set_visible(false);update_dialogue();
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation);
    _pause_menu->set_visible(true);_pause_menu->set_active(true);_pause_menu->focus_first_available_scope();
}
void FlightScene::close_pause_menu(){
    if(!_pause_menu_open)return;
    _pause_menu->set_visible(false);_pause_menu->set_active(false);_pause_menu_open=false;
    for(auto* object:_pause_updates)object->set_update_when_paused(true);_pause_updates.clear();
    if(!_pause_was_logic_paused)resume();
    _pause_button->set_active(true);set_all_gameplay_input_blocked(false);set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);update_dialogue();
}
void FlightScene::retry_level(){request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);}
void FlightScene::show_result(bool saved){
    _result_title->set_text_content(ui::ui_raw_text(content::format(_success?"result_success":"result_failure",_level)));
    _result_stars->set_collected(_stars);
    const int score=_success?progress::original_score(_level,_stars,_crystals,_elapsed,content::catalog().at("score").at("svrs")):0;
    _result_stats->set_text_content(ui::ui_raw_text(content::format("result_stats",_elapsed,_crystals,score)));
    _result_save->set_text_content(ui::ui_raw_text(saved?"":content::ui("save_failure")));
    _result_continue->set_text_content(ui::ui_raw_text(content::ui(_level==60?"credits":"next_level")));
    _result_continue->set_visible(_success);_result_continue->set_enabled(_success);
    _pause_button->set_visible(false);_hint->set_visible(false);
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation);
    _result->set_visible(true);_result->set_active(true);_result->focus_first_available_scope();
}
void FlightScene::continue_from_result(){
    if(!_finished)return;
    if(!_success)retry_level();
    else if(_level==60)request_scene_switch(scene_keys::Credits);
    else if(progress::Store::instance().data.unlocked(_level+1,content::catalog().at("dependencies")))request_scene_switch(scene_keys::Flight,FlightPayload{_level+1},elysia::scene::SceneReloadMode::Recreate);
    else request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});
}
}
