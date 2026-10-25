#include "flight_scene.h"
#include "selection_scene.h"
#include "credits_scene.h"
#include "game/progress/progress.h"
#include "game/progress/scoring.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/resources/resource_service.h"

namespace game::scene {
using namespace elysia;
void FlightScene::build_result_panel() {
    _result=create_and_add_object<ui::UiWindow>(core::Rect{300,120,680,480},30);
    _result->set_style_overrides({.background=core::Color{12,23,51,248}});
    _result->set_on_cancel([this]{request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,600,440});
    list->set_cross_align(ui::UiLayoutAlign::Center);list->set_item_spacing(12);
    auto label=[&](float height){
        auto element=std::make_unique<ui::UiLabel>(core::Rect{0,0,600,height},0,ui::ui_raw_text(""));
        auto* ptr=element.get();ptr->set_horizontal_align(ui::TextHorizontalAlign::Center);
        ptr->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);list->add_back(std::move(element));return ptr;
    };
    _result_title=label(48);_result_title->set_typography_role(typography::UiTypographyRole::Title);
    auto stars=std::make_unique<ui::UiListContainer>(core::Rect{0,0,216,64});
    stars->set_direction(ui::UiListDirection::Horizontal);stars->set_item_spacing(12);
    for(auto& ptr:_result_stars){auto star=std::make_unique<ui::UiImage>(nullptr,core::Rect{0,0,64,64});ptr=star.get();stars->add_back(std::move(star));}
    list->add_back(std::move(stars));_result_stats=label(40);_result_save=label(32);
    auto button=[&](const char* key,std::function<void()> action){
        auto element=std::make_unique<ui::UiButton>(core::Rect{0,0,440,48});auto* ptr=element.get();
        ptr->set_text_content(ui::ui_raw_text(content::ui(key)));ptr->set_on_click(std::move(action));list->add_back(std::move(element));return ptr;
    };
    _result_continue=button("next_level",[this]{continue_from_result();});
    button("retry",[this]{request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);});
    button("select_level",[this]{request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});});
    auto* scope=list.get();_result->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    _result->register_focus_scope(*scope);_result->set_visible(false);_result->set_active(false);
}
void FlightScene::show_result(bool saved) {
    _result_title->set_text_content(ui::ui_raw_text(content::format(_success?"result_success":"result_failure",_level)));
    for(int i=0;i<3;++i){
        _result_stars[i]->set_texture(ELYSIA_RESOURCES->find_texture(i<_stars?"fly.InGameUI.starUp":"fly.InGameUI.starDown"));
        _result_stars[i]->set_opacity(i<_stars?255:55);
    }
    const int score=_success?progress::original_score(_level,_stars,_crystals,_elapsed,content::catalog().at("score").at("svrs")):0;
    _result_stats->set_text_content(ui::ui_raw_text(content::format("result_stats",_elapsed,_crystals,score,_stars)));
    _result_save->set_text_content(ui::ui_raw_text(saved?"":content::ui("save_failure")));
    _result_continue->set_text_content(ui::ui_raw_text(content::ui(!_success?"retry":_level==60?"credits":"next_level")));
    _result_continue->set_visible(_success);_result_continue->set_enabled(_success);
    _result->set_visible(true);_result->set_active(true);_result->focus_first_available_scope();
}
void FlightScene::continue_from_result() {
    if(!_finished)return;
    if(!_success)request_scene_switch(scene_keys::Flight,FlightPayload{_level,true},elysia::scene::SceneReloadMode::Recreate);
    else if(_level==60)request_scene_switch(scene_keys::Credits);
    else if(progress::Store::instance().data.unlocked(_level+1,content::catalog().at("dependencies")))request_scene_switch(scene_keys::Flight,FlightPayload{_level+1},elysia::scene::SceneReloadMode::Recreate);
    else request_scene_switch(scene_keys::Selection,SelectionPayload{(_level-1)/15});
}
}
