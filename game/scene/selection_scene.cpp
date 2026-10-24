#include "selection_scene.h"
#include "main_menu_scene.h"
#include "flight_scene.h"
#include "game/progress/progress.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/window/ui_window.h"
#include <format>
#include <numeric>
namespace game::scene {
using namespace elysia;
void SelectionScene::on_after_update(double){if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",3}}))request_quit();}
void AchievementsScene::on_after_update(double){if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",4}}))request_quit();}
namespace {
std::unique_ptr<ui::UiButton> button(std::string text,float width,float height,std::function<void()> action) {
    auto b=std::make_unique<ui::UiButton>(core::Rect{0,0,width,height});b->set_text_content(ui::ui_raw_text(std::move(text)));b->set_on_click(std::move(action));return b;
}
void title(ui::UiListContainer& list,const std::string& text) {
    auto t=std::make_unique<ui::UiLabel>(core::Rect{0,0,1120,56},0,ui::ui_raw_text(text));
    t->set_typography_role(typography::UiTypographyRole::Title);t->set_horizontal_align(ui::TextHorizontalAlign::Center);list.add_back(std::move(t));
}
}
void SelectionScene::on_enter(const elysia::scene::ScenePayload& payload) {
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    int chapter=0;if(auto p=elysia::scene::try_scene_payload<SelectionPayload>(payload))chapter=std::clamp(p->chapter,0,3);
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720});window->set_on_cancel([this]{request_scene_switch(scene_keys::MainMenu);});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,1120,640});list->set_item_spacing(18);
    title(*list,content::format("chapter_title",chapter+1));
    auto chapters=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,1120,48});chapters->set_column_count(4);chapters->set_cell_spacing({16,0});
    for(int c=0;c<4;++c)chapters->add_child(button(content::format("chapter_range",c+1,c*15+1,c*15+15),265,48,[this,c]{request_scene_switch(scene_keys::Selection,SelectionPayload{c},elysia::scene::SceneReloadMode::Recreate);}));
    list->add_back(std::move(chapters));
    auto levels=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,1120,330});levels->set_column_count(5);levels->set_cell_spacing({16,18});
    for(int id=chapter*15+1;id<=chapter*15+15;++id) {
        const bool open=store.data.unlocked(id,content::catalog().at("dependencies"));const int stars=store.data.stars[id-1];
        const std::string status=!open?content::ui("locked"):stars<0?content::ui("challenge"):content::format("stars",stars);
        auto b=button(content::format("level_button",id,status),210,94,[this,id]{request_scene_switch(scene_keys::Flight,FlightPayload{id},elysia::scene::SceneReloadMode::Recreate);});b->set_enabled(open);levels->add_child(std::move(b));
    }
    auto* levels_scope=levels.get();list->add_back(std::move(levels));
    const int cleared=static_cast<int>(std::count_if(store.data.stars.begin(),store.data.stars.end(),[](int s){return s>=0;}));
    std::string summary=content::format("progress",cleared,std::accumulate(store.data.scores.begin(),store.data.scores.end(),std::int64_t{0}),store.data.achievements.size());
    if(!store.error.empty())summary=content::format("save_error",store.error);else if(store.recovered)summary+=content::ui("save_recovered");
    list->add_back(std::make_unique<ui::UiLabel>(core::Rect{0,0,1120,40},0,ui::ui_raw_text(summary)));
    list->add_back(button(content::ui("back"),1120,50,[this]{request_scene_switch(scene_keys::MainMenu);}));
    auto* list_scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    // The list delegates focus to its grids. Registering those grids again on
    // the window lets the parent and window clear each other's pressed state.
    if(levels_scope->focus_first_available())list_scope->set_focused_target(levels_scope->focused_target());
    window->register_focus_scope(*list_scope);window->focus_first_available_scope();
}
void AchievementsScene::on_enter(const elysia::scene::ScenePayload&) {
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    auto* window=create_and_add_object<ui::UiWindow>(core::Rect{0,0,1280,720});window->set_on_cancel([this]{request_scene_switch(scene_keys::MainMenu);});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,1160,660});list->set_item_spacing(14);
    title(*list,content::format("achievements_title",store.data.achievements.size()));
    auto grid=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,1160,510});grid->set_column_count(2);grid->set_cell_spacing({22,6});
    const auto descriptions=content::read("text/"+content::language()+"/gcDescriptions.json");
    for(const auto& [key,a]:descriptions.at("achievements").items()) {
        auto label=std::make_unique<ui::UiLabel>(core::Rect{0,0,565,48},0,ui::ui_raw_text(std::format("{} {} · {}",store.data.achievements.contains(std::stoi(key))?content::ui("earned"):content::ui("not_earned"),a.at("title").get<std::string>(),a.at("earnedDesc").get<std::string>())));
        label->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);grid->add_child(std::move(label));
    }
    list->add_back(std::move(grid));list->add_back(button(content::ui("back"),1160,50,[this]{request_scene_switch(scene_keys::MainMenu);}));
    auto* scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});window->register_focus_scope(*scope);window->focus_first_available_scope();
}
}
