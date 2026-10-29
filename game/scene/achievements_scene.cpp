#include "selection_scene.h"
#include "main_menu_scene.h"
#include "game/progress/progress.h"
#include "game/testing/replay_probe.h"
#include "engine/audio/audio_service.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/window/ui_window.h"

namespace game::scene {
using namespace elysia;
namespace {
constexpr int achievements_per_page=6;
constexpr float margin=24,gap=16,title_height=56,page_height=56,back_height=67;
}
void AchievementsScene::on_enter(const elysia::scene::ScenePayload& payload){
    ELYSIA_AUDIO->transition_music("fly.SkyTunnel");
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    const auto descriptions=content::read("text/"+content::language()+"/gcDescriptions.json").at("achievements");
    const int page_count=std::max(1,(static_cast<int>(descriptions.size())+achievements_per_page-1)/achievements_per_page);
    const auto* requested=elysia::scene::try_scene_payload<AchievementsPayload>(payload);
    _page=std::clamp(requested?requested->page:0,0,page_count-1);
    const core::Rect viewport{0,0,static_cast<float>(runtime_context().logical_width()),static_cast<float>(runtime_context().logical_height())};
    const bool portrait=viewport.height()>=viewport.width();
    const float width=std::min(portrait?592.f:1160.f,viewport.width()-2*margin),height=viewport.height()-2*margin;
    const int columns=portrait?2:3,rows=achievements_per_page/columns;
    const float grid_height=height-title_height-page_height-back_height-3*gap;
    const core::Vector2 cell{(width-(columns-1)*16)/columns,(grid_height-(rows-1)*12)/rows};
    auto* window=create_and_add_object<ui::UiWindow>(viewport);
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    window->set_on_cancel([this]{game::audio::ui_click();request_scene_switch(scene_keys::MainMenu);});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,width,height});
    list->set_item_spacing(gap);list->set_cross_align(ui::UiLayoutAlign::Center);
    auto title=frontend::label(content::format("achievements_title",store.data.achievements.size()),width,title_height,true);
    title->set_typography_role(typography::UiTypographyRole::Heading);
    title->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);list->add_back(std::move(title));
    auto cards=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,width,grid_height});
    cards->set_column_count(columns);cards->set_cell_spacing({16,12});
    int index=0;
    for(const auto& [key,description]:descriptions.items()){
        if(index++/achievements_per_page!=_page)continue;
        const int id=std::stoi(key);
        auto card=std::make_unique<frontend::AchievementCard>(id,description.at("title"),description.at("earnedDesc"),store.data.achievements.contains(id));
        card->set_size(cell);cards->add_child(std::move(card));
    }
    list->add_back(std::move(cards));
    auto pages=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,width,page_height});
    pages->set_column_count(page_count);pages->set_cell_spacing({12,0});
    auto* page_scope=pages.get();ui::UiButton* current_page=nullptr;
    const float page_width=(width-(page_count-1)*12)/page_count;
    for(int page=0;page<page_count;++page){
        auto button=std::make_unique<frontend::GlassButton>(content::format("album_page",page+1),page_width,page_height,[this,page]{
            request_scene_switch(scene_keys::Achievements,AchievementsPayload{page},elysia::scene::SceneReloadMode::Recreate);
        });
        if(page==_page){frontend::mark_selected(*button);current_page=button.get();}
        pages->add_child(std::move(button));
    }
    list->add_back(std::move(pages));
    list->add_back(std::make_unique<frontend::GlassButton>(content::ui("back"),345.f,back_height,[this]{request_scene_switch(scene_keys::MainMenu);}));
    auto* scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    page_scope->set_focused_target(current_page);scope->set_focused_target(current_page);
    window->register_focus_scope(*scope);window->focus_first_available_scope();
    window->add_child(std::make_unique<frontend::MoonBackdrop>(runtime_context(),"moon",145));
}
void AchievementsScene::on_after_update(double){
    const auto& store=progress::Store::instance();
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",4},{"achievement_page",_page},{"achievement_count",store.data.achievements.size()},{"achievement101",store.data.achievements.contains(101)},{"save_error",store.error}}))request_quit();
}
}
