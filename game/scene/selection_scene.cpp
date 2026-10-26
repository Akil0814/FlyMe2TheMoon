#include "selection_scene.h"
#include "main_menu_scene.h"
#include "flight_scene.h"
#include "game/progress/progress.h"
#include "game/frontend/moon_ui.h"
#include "engine/audio/audio_service.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/window/ui_window.h"
#include <format>
#include <numeric>
namespace game::scene {
using namespace elysia;
void SelectionScene::on_after_update(double){
    const auto& store=progress::Store::instance();
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",3},{"cleared",std::count_if(store.data.stars.begin(),store.data.stars.end(),[](int s){return s>=0;})},{"stars_level1",store.data.stars[0]},{"score_level1",store.data.scores[0]},{"score_verified_level1",store.data.score_verified[0]},{"achievement101",store.data.achievements.contains(101)},{"save_error",store.error}}))request_quit();
}
void AchievementsScene::on_after_update(double){
    const auto& store=progress::Store::instance();
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",4},{"achievement_page",_page},{"achievement_count",store.data.achievements.size()},{"achievement101",store.data.achievements.contains(101)},{"save_error",store.error}}))request_quit();
}
namespace {
std::unique_ptr<ui::UiButton> button(std::string text,float width,float height,std::function<void()> action) {
    return std::make_unique<frontend::GlassButton>(std::move(text),width,height,std::move(action));
}
void title(ui::UiListContainer& list,const std::string& text) {
    auto t=std::make_unique<ui::UiLabel>(core::Rect{0,0,1120,56},0,ui::ui_raw_text(text));
    t->set_typography_role(typography::UiTypographyRole::Heading);t->set_horizontal_align(ui::TextHorizontalAlign::Center);t->set_style_overrides({.text=core::Color{237,251,255,255}});list.add_back(std::move(t));
}
}
void SelectionScene::on_enter(const elysia::scene::ScenePayload& payload) {
    ELYSIA_AUDIO->transition_music("fly.MoonTrip");
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    int chapter=0;if(auto p=elysia::scene::try_scene_payload<SelectionPayload>(payload))chapter=std::clamp(p->chapter,0,3);
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720});window->set_on_cancel([this]{request_scene_switch(scene_keys::MainMenu);});
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,1120,640});list->set_item_spacing(18);
    title(*list,content::format("chapter_title",chapter+1));
    auto chapters=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,1120,48});chapters->set_column_count(4);chapters->set_cell_spacing({16,0});
    auto* chapter_scope=chapters.get();ui::UiButton* current_chapter=nullptr;
    for(int c=0;c<4;++c){auto tab=button(content::format("chapter_range",c+1,c*15+1,c*15+15),265,48,[this,c]{request_scene_switch(scene_keys::Selection,SelectionPayload{c},elysia::scene::SceneReloadMode::Recreate);});
        if(c==chapter){frontend::mark_selected(*tab);current_chapter=tab.get();}
        chapters->add_child(std::move(tab));}
    list->add_back(std::move(chapters));
    auto levels=std::make_unique<frontend::ConstellationGrid>(core::Rect{0,0,1120,330});levels->set_column_count(5);levels->set_cell_spacing({16,18});
    for(int id=chapter*15+1;id<=chapter*15+15;++id) {
        const bool open=store.data.unlocked(id,content::catalog().at("dependencies"));const int stars=store.data.stars[id-1];
        const auto score=stars>=0&&store.data.score_verified[id-1]
            ?std::optional<int>{store.data.scores[id-1]}:std::nullopt;
        levels->add_child(std::make_unique<frontend::LevelNode>(id,chapter,stars,open,[this,id]{request_scene_switch(scene_keys::Flight,FlightPayload{id},elysia::scene::SceneReloadMode::Recreate);},score));
    }
    auto* levels_scope=levels.get();list->add_back(std::move(levels));
    const int cleared=static_cast<int>(std::count_if(store.data.stars.begin(),store.data.stars.end(),[](int s){return s>=0;}));
    std::string summary=content::format("progress",cleared,std::accumulate(store.data.scores.begin(),store.data.scores.end(),std::int64_t{0}),store.data.achievements.size());
    if(!store.error.empty())summary=content::format("save_error",store.error);else if(store.recovered)summary+=content::ui("save_recovered");
    list->add_back(frontend::label(summary,1120,40,true));
    list->add_back(button(content::ui("back"),320,50,[this]{request_scene_switch(scene_keys::MainMenu);}));list->set_cross_align(ui::UiLayoutAlign::Center);
    auto* list_scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    // The list delegates focus to its grids. Registering those grids again on
    // the window lets the parent and window clear each other's pressed state.
    if(levels_scope->focus_first_available())list_scope->set_focused_target(levels_scope->focused_target());
    else if(current_chapter){chapter_scope->set_focused_target(current_chapter);list_scope->set_focused_target(current_chapter);}
    window->register_focus_scope(*list_scope);window->focus_first_available_scope();
    window->add_child(std::make_unique<frontend::MoonBackdrop>("moon",110));
}
void AchievementsScene::on_enter(const elysia::scene::ScenePayload& payload) {
    ELYSIA_AUDIO->transition_music("fly.MoonTrip");
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    _page=0;if(auto* p=elysia::scene::try_scene_payload<AchievementsPayload>(payload))_page=std::clamp(p->page,0,2);
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720});window->set_on_cancel([this]{request_scene_switch(scene_keys::MainMenu);});
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,1160,652});list->set_item_spacing(16);list->set_cross_align(ui::UiLayoutAlign::Center);
    title(*list,content::format("achievements_title",store.data.achievements.size()));
    list->add_back(std::make_unique<ui::UiElement>(core::Rect{0,0,1120,30}));
    auto grid=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,1160,374});grid->set_column_count(3);grid->set_cell_spacing({20,18});
    const auto descriptions=content::read("text/"+content::language()+"/gcDescriptions.json");
    int index=0;
    for(const auto& [key,a]:descriptions.at("achievements").items()) {
        if(index++/6!=_page)continue;
        grid->add_child(std::make_unique<frontend::AchievementCard>(std::stoi(key),a.at("title"),a.at("earnedDesc"),store.data.achievements.contains(std::stoi(key))));
    }
    list->add_back(std::move(grid));
    auto pages=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,560,44});pages->set_column_count(3);pages->set_cell_spacing({16,0});
    auto* page_scope=pages.get();ui::UiButton* current_page=nullptr;
    for(int p=0;p<3;++p){auto b=button(content::format("album_page",p+1),176,44,[this,p]{request_scene_switch(scene_keys::Achievements,AchievementsPayload{p},elysia::scene::SceneReloadMode::Recreate);});
        if(p==_page){frontend::mark_selected(*b);current_page=b.get();}
        pages->add_child(std::move(b));}
    list->add_back(std::move(pages));
    list->add_back(button(content::ui("back"),320,48,[this]{request_scene_switch(scene_keys::MainMenu);}));
    auto* scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    page_scope->set_focused_target(current_page);scope->set_focused_target(current_page);
    window->register_focus_scope(*scope);window->focus_first_available_scope();
    window->add_child(std::make_unique<frontend::MoonBackdrop>("moon",145));
}
}
