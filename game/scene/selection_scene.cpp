#include "game/audio/game_audio.h"
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
void SelectionScene::on_after_update(double delta){
    _audio.advance(delta);_unlock_time-=std::max(0.,delta);
    if(!_unlock_nodes.empty()&&_unlock_time<=0){
        _unlock_nodes.front().second->show_unlock();progress::Store::instance().pending_unlocks.erase(_unlock_nodes.front().first);
        _unlock_nodes.erase(_unlock_nodes.begin());_unlock_time=.5;_audio.play("108-1",1,true);
    }
    const auto& store=progress::Store::instance();
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",3},{"cleared",std::count_if(store.data.stars.begin(),store.data.stars.end(),[](int s){return s>=0;})},{"stars_level1",store.data.stars[0]},{"score_level1",store.data.scores[0]},{"score_verified_level1",store.data.score_verified[0]},{"achievement101",store.data.achievements.contains(101)},{"save_error",store.error}}))request_quit();
}

namespace {
std::unique_ptr<ui::UiButton> button(std::string text,float width,float height,std::function<void()> action) {
    return std::make_unique<frontend::GlassButton>(std::move(text),width,height,std::move(action));
}
struct SelectionLayout {
    core::Rect viewport;
    float width,height,chapter_height,level_height;
    int chapter_columns,level_columns,level_rows;
    core::Vector2 chapter_cell,level_cell;
};
SelectionLayout selection_layout(const elysia::scene::SceneRuntimeContext& context){
    SelectionLayout layout{};
    layout.viewport={0,0,static_cast<float>(context.logical_width()),static_cast<float>(context.logical_height())};
    const bool portrait=layout.viewport.height()>=layout.viewport.width();
    layout.width=std::min(portrait?592.f:1120.f,layout.viewport.width()-48);layout.height=layout.viewport.height()-48;
    layout.chapter_columns=portrait?2:4;layout.level_columns=portrait?3:5;layout.level_rows=15/layout.level_columns;
    layout.chapter_height=portrait?124.f:56.f;
    layout.level_height=layout.height-56-layout.chapter_height-48-67-4*16;
    layout.chapter_cell={(layout.width-(layout.chapter_columns-1)*12)/layout.chapter_columns,56};
    layout.level_cell={(layout.width-(layout.level_columns-1)*16)/layout.level_columns,(layout.level_height-(layout.level_rows-1)*12)/layout.level_rows};
    return layout;
}
std::unique_ptr<ui::UiGridContainer> chapter_tabs(const SelectionLayout& layout,int chapter,
    std::function<void(int)> select,ui::UiButton*& current){
    auto grid=std::make_unique<ui::UiGridContainer>(core::Rect{0,0,layout.width,layout.chapter_height});
    grid->set_column_count(layout.chapter_columns);grid->set_cell_spacing({12,12});
    for(int c=0;c<4;++c){
        auto tab=button(content::format("chapter_range",c+1,c*15+1,c*15+15),layout.chapter_cell.x,layout.chapter_cell.y,[select,c]{select(c);});
        if(c==chapter){frontend::mark_selected(*tab);current=tab.get();}
        grid->add_child(std::move(tab));
    }
    return grid;
}
std::unique_ptr<frontend::ConstellationGrid> level_grid(const SelectionLayout& layout,int chapter,const progress::Store& store,
    std::function<void(int)> select,std::vector<std::pair<int,frontend::LevelNode*>>& unlock_nodes){
    auto grid=std::make_unique<frontend::ConstellationGrid>(core::Rect{0,0,layout.width,layout.level_height});
    grid->set_column_count(layout.level_columns);grid->set_cell_spacing({16,12});
    for(int id=chapter*15+1;id<=chapter*15+15;++id){
        const bool open=store.data.unlocked(id,content::catalog().at("dependencies"));const int stars=store.data.stars[id-1];
        const auto score=stars>=0&&store.data.score_verified[id-1]?std::optional<int>{store.data.scores[id-1]}:std::nullopt;
        auto node=std::make_unique<frontend::LevelNode>(id,stars,open,[select,id]{select(id);},score);
        node->set_size(layout.level_cell);
        if(open&&store.pending_unlocks.contains(id))unlock_nodes.emplace_back(id,node.get());
        grid->add_child(std::move(node));
    }
    return grid;
}
std::unique_ptr<ui::UiLabel> progress_summary(const progress::Store& store,float width){
    const int cleared=static_cast<int>(std::count_if(store.data.stars.begin(),store.data.stars.end(),[](int s){return s>=0;}));
    auto text=content::format("progress",cleared,std::accumulate(store.data.scores.begin(),store.data.scores.end(),std::int64_t{0}),store.data.achievements.size());
    if(!store.error.empty())text=content::format("save_error",store.error);else if(store.recovered)text+=content::ui("save_recovered");
    auto label=frontend::label(text,width,48,true);label->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);return label;
}
}
void SelectionScene::on_enter(const elysia::scene::ScenePayload& payload) {
    _audio.reset();_unlock_nodes.clear();_unlock_time=.25;
    ELYSIA_AUDIO->transition_music("fly.SkyTunnel");
    clear_scene_objects();auto& store=progress::Store::instance();(void)store.load();
    int chapter=0;if(auto p=elysia::scene::try_scene_payload<SelectionPayload>(payload))chapter=std::clamp(p->chapter,0,3);
    const auto layout=selection_layout(runtime_context());
    auto* window=create_and_add_object<ui::UiWindow>(layout.viewport);window->set_on_cancel([this]{game::audio::ui_click();request_scene_switch(scene_keys::MainMenu);});
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    auto list=std::make_unique<ui::UiListContainer>(core::Rect{0,0,layout.width,layout.height});list->set_item_spacing(16);
    auto title=frontend::label(content::format("chapter_title",chapter+1),layout.width,56,true);
    title->set_typography_role(typography::UiTypographyRole::Heading);title->set_horizontal_align(ui::TextHorizontalAlign::Center);
    title->set_style_overrides({.text=core::Color{237,251,255,255}});title->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);list->add_back(std::move(title));
    ui::UiButton* current_chapter=nullptr;
    auto chapters=chapter_tabs(layout,chapter,[this](int c){request_scene_switch(scene_keys::Selection,SelectionPayload{c},elysia::scene::SceneReloadMode::Recreate);},current_chapter);
    auto* chapter_scope=chapters.get();
    list->add_back(std::move(chapters));
    auto levels=level_grid(layout,chapter,store,[this](int id){request_scene_switch(scene_keys::Flight,FlightPayload{id},elysia::scene::SceneReloadMode::Recreate);},_unlock_nodes);
    auto* levels_scope=levels.get();list->add_back(std::move(levels));
    list->add_back(progress_summary(store,layout.width));
    list->add_back(button(content::ui("back"),345,67,[this]{request_scene_switch(scene_keys::MainMenu);}));list->set_cross_align(ui::UiLayoutAlign::Center);
    auto* list_scope=list.get();window->add_child(std::move(list),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::Center});
    // The list delegates focus to its grids. Registering those grids again on
    // the window lets the parent and window clear each other's pressed state.
    if(levels_scope->focus_first_available())list_scope->set_focused_target(levels_scope->focused_target());
    else if(current_chapter){chapter_scope->set_focused_target(current_chapter);list_scope->set_focused_target(current_chapter);}
    window->register_focus_scope(*list_scope);window->focus_first_available_scope();
    window->add_child(std::make_unique<frontend::MoonBackdrop>(runtime_context(),"moon",110));
}
}
