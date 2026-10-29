#include "game/audio/game_audio.h"
#include "credits_scene.h"
#include "main_menu_scene.h"
#include "game/content/content.h"
#include "game/frontend/moon_ui.h"
#include "game/testing/replay_probe.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/image/ui_image.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/resources/resource_service.h"
#include "engine/audio/audio_service.h"

namespace game::scene {
using namespace elysia;
void CreditsScene::on_enter(const elysia::scene::ScenePayload&) {
    clear_scene_objects();_auto_scroll=true;_wait=2;testing::ReplayProbe::shared().begin();
    const core::Rect viewport{0,0,static_cast<float>(runtime_context().logical_width()),static_cast<float>(runtime_context().logical_height())};
    constexpr float margin=24,controls_height=109;
    const float page_width=std::min(592.f,viewport.width()-2*margin),image_width=page_width-48;
    auto* window=create_and_add_object<ui::UiWindow>(viewport);
    window->set_style_overrides({.draw_background=false,.draw_border=false});
    window->add_child(std::make_unique<frontend::MoonBackdrop>(runtime_context(),"settings",100));
    window->set_on_cancel([this]{game::audio::ui_click();request_scene_switch(scene_keys::MainMenu);});
    auto scroll=std::make_unique<ui::UiScrollContainer>(core::Rect{0,0,page_width,viewport.height()-2*margin-controls_height-16});_scroll=scroll.get();
    scroll->set_scroll_axis(ui::UiScrollAxis::Vertical);
    scroll->set_style_overrides({.scrollbar={.corner_radius=7.f,.thickness=14.f,.margin=7.f,.min_thumb_length=40.f,
        .track_idle_color=core::Color{12,49,91,100},.track_focused_color=core::Color{29,80,126,130},
        .track_dragging_color=core::Color{29,105,164,150},.track_disabled_color=core::Color{12,49,91,60},
        .thumb_idle_color=core::Color{112,204,245,210},.thumb_focused_color=core::Color{174,235,255,240},
        .thumb_dragging_color=core::Color{219,249,255,255},.thumb_disabled_color=core::Color{112,204,245,80}},
        .draw_background=false,.draw_border=false});
    auto images=std::make_unique<ui::UiListContainer>(core::Rect{0,0,page_width,1});images->set_cross_align(ui::UiLayoutAlign::Center);images->set_item_spacing(28);
    images->set_padding({24,0,24,0});
    for(const auto* name:{"creditsText1","creditsText2","creditsText3","creditsText4","creditsText5","creditsText6","creditsSleep"}) {
        auto* texture=ELYSIA_RESOURCES->find_texture(std::string("fly.ui.")+name);float width=0,height=0;
        if(!texture||!SDL_GetTextureSize(texture,&width,&height))throw std::runtime_error("Missing credits image");
        const float scale=std::min(1.5f,image_width/width);
        images->add_back(std::make_unique<ui::UiImage>(texture,core::Rect{0,0,width*scale,height*scale}));
    }
    scroll->set_content(std::move(images));window->add_child(std::move(scroll),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::TopCenter,._margin={.top=margin}});
    auto controls=std::make_unique<ui::UiListContainer>(core::Rect{0,0,page_width,controls_height});controls->set_cross_align(ui::UiLayoutAlign::Center);controls->set_item_spacing(12);
    auto hint=std::make_unique<ui::UiLabel>(core::Rect{0,0,page_width,30},0,ui::ui_raw_text(content::ui("credits_controls")));hint->set_horizontal_align(ui::TextHorizontalAlign::Center);hint->set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);controls->add_back(std::move(hint));
    controls->add_back(std::make_unique<frontend::GlassButton>(content::ui("back"),345,67,[this]{request_scene_switch(scene_keys::MainMenu);}));
    auto* scope=controls.get();window->add_child(std::move(controls),ui::UiLayoutChildOptions{._anchor=ui::UiLayoutAnchor::BottomCenter,._margin={.bottom=margin}});window->register_focus_scope(*scope);window->focus_first_available_scope();
    ELYSIA_AUDIO->transition_music("fly.MoonTrip");
}
void CreditsScene::on_exit(){_scroll=nullptr;clear_scene_objects();}
void CreditsScene::on_shortcuts(const input::RawInputFrame&,const std::vector<input::RawInputEvent>& events) {
    using C=input::RawInputControl;
    for(const auto& e:events)if(e.type==input::RawInputEventType::ControlPressed){
        if(e.control==C::KeySpace||e.control==C::GamepadWest){consume_input(e);_auto_scroll=!_auto_scroll;}
        else if(e.control==C::KeyDown||e.control==C::KeyUp){consume_input(e);_auto_scroll=false;_scroll->scroll_by({0,e.control==C::KeyDown?90.f:-90.f});}
        else if(e.control==C::MouseLeft)_auto_scroll=false;
    }
}
void CreditsScene::on_after_update(double delta){
    _wait-=delta;if(_auto_scroll&&_wait<=0)_scroll->scroll_by({0,static_cast<float>(delta*30)});
    if(testing::ReplayProbe::shared().update(runtime_context().renderer(),{{"scene",5},{"scroll_y",_scroll->scroll_offset_y()},{"scroll_max",_scroll->max_scroll_offset().y},{"auto_scroll",_auto_scroll}}))request_quit();
}
}
