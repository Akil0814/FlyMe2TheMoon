#include "moon_ui.h"
#include "result_star_effects.h"
#include "game/audio/game_audio.h"
#include "engine/resources/resource_service.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "game/content/content.h"
#include <format>
#include <cmath>
#include <cctype>

namespace game::frontend {
using namespace elysia;
namespace {
SDL_Texture* texture(const std::string& key){return ELYSIA_RESOURCES->find_texture("fly.frontend."+key);}
std::string level_color(int id){
    // Original LevelSelectData stores [x, y, type]; type 1/2/3/4 is P/Y/O/G.
    const int type=content::catalog().at("selection").at("levels").at(std::to_string(id)).at(2).get<int>();
    return std::string(1,std::array{'P','Y','O','G'}.at(type-1));
}
void image(std::vector<core::UiRenderCommand>& out,SDL_Texture* source,core::Rect rect,Uint8 alpha=255){
    if(!source)return;
    auto command=core::make_ui_texture_command(source,rect);command.alpha=alpha;out.push_back(command);
}
void hud_background(std::vector<core::UiRenderCommand>& out,core::Rect rect){
    auto* source=ELYSIA_RESOURCES->find_texture("fly.InGameUI.btn1Up");float width=0,height=0;
    if(!source||!SDL_GetTextureSize(source,&width,&height)||rect.is_empty())return;
    // Stretch the center while retaining the rounded end caps at the target height.
    const float source_cap=std::floor(std::min(height*.35f,width/2)),cap=std::floor(std::min(rect.height()*.35f,rect.width()/2));
    const std::array<core::Rect,3> sources{{{0,0,source_cap,height},{source_cap,0,width-2*source_cap,height},{width-source_cap,0,source_cap,height}}};
    const std::array<core::Rect,3> targets{{{rect.x(),rect.y(),cap,rect.height()},{rect.x()+cap,rect.y(),rect.width()-2*cap,rect.height()},{rect.right()-cap,rect.y(),cap,rect.height()}}};
    for(std::size_t i=0;i<3;++i)if(!targets[i].is_empty()){
        auto command=core::make_ui_texture_command(source,targets[i]);command.use_src_rect=true;command.src_rect=sources[i];out.push_back(command);
    }
}
ui::UiButtonStyleOverrides glass_button_style(){
    return {.chrome={.background={.idle=core::Color{12,49,91,90},.focused=core::Color{72,182,245,80},.active=core::Color{29,105,164,130}},
        .border={.idle=core::Color{125,216,255,75},.focused=core::Color{204,247,255,255}},.corner_radius=10.f,.draw_border=true},
        .text={.enabled=core::Color{241,251,255,255}}};
}
void draw_glass_button(const ui::UiButton& button,std::vector<core::UiRenderCommand>& out){
    if(!button.is_visible())return;
    image(out,texture("mainMenuBtnUp"),button.screen_rect(),button.is_enabled()?220:80);
    button.ui::UiButton::submit_ui_render_commands(out);
}
}
void StarRating::present_result(int count,double elapsed){
    _result_count=std::clamp(count,0,3);_result_elapsed=std::max(0.,elapsed);_collected=0;
    while(_collected<_result_count&&_result_elapsed+1e-9>=_collected*result_star_interval+result_star_reveal_delay)++_collected;
}
core::Rect StarRating::slot_rect(int index)const{
    const auto r=screen_rect();const float gap=r.height()*.1875f;
    const float side=std::min(r.height(),(r.width()-gap*2)/3);
    const float x=r.center().x-(side*3+gap*2)/2,y=r.center().y-side/2;
    return {x+(side+gap)*std::clamp(index,0,2),y,side,side};
}
void StarRating::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    for(int index=0;index<3;++index){
        auto* star=ELYSIA_RESOURCES->find_texture(index<_collected?"fly.InGameUI.starDown":"fly.InGameUI.starUp");
        image(out,star,slot_rect(index));
    }
    for(int index=0;index<_result_count;++index)
        submit_result_star_particles(out,slot_rect(index),_result_elapsed-index*result_star_interval,index);
}
HudLabel::HudLabel(core::Rect rect,int order,ui::UiTextContent text):UiLabel(rect,order,std::move(text)){
    set_typography_role(typography::UiTypographyRole::ButtonCompact);
    set_style_overrides({.text=core::Color{16,46,77,255},.draw_background=false});
    set_padding(8);set_vertical_align(ui::TextVerticalAlign::Center);set_horizontal_align(ui::TextHorizontalAlign::Center);
    set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);
}
void HudLabel::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const{
    if(!is_visible())return;
    hud_background(out,screen_rect());UiLabel::submit_ui_render_commands(out);
}
void HudPanel::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const{
    if(!is_visible())return;
    auto* source=ELYSIA_RESOURCES->find_texture("fly.InGameUI.btn1Up");float width=0,height=0;
    const auto r=screen_rect();if(!source||!SDL_GetTextureSize(source,&width,&height)||r.is_empty())return;
    // Large dialogue and modal frames retain fixed corners in both dimensions.
    const float edge=std::floor(std::min({height/3,width/3,24.f}));
    const float target=std::floor(std::min({r.width()/2,r.height()/2,edge}));
    const std::array<float,4> sx{{0,edge,width-edge,width}},sy{{0,edge,height-edge,height}};
    const std::array<float,4> dx{{r.x(),r.x()+target,r.right()-target,r.right()}},dy{{r.y(),r.y()+target,r.bottom()-target,r.bottom()}};
    for(int row=0;row<3;++row)for(int column=0;column<3;++column){
        const core::Rect rect{dx[column],dy[row],dx[column+1]-dx[column],dy[row+1]-dy[row]};if(rect.is_empty())continue;
        auto command=core::make_ui_texture_command(source,rect);command.use_src_rect=true;
        command.src_rect={sx[column],sy[row],sx[column+1]-sx[column],sy[row+1]-sy[row]};out.push_back(command);
    }
}
RunStats::RunStats(core::Rect rect,int order):UiElement(rect,order){
    for(auto& label:_labels){
        label.set_typography_role(typography::UiTypographyRole::ButtonCompact);
        label.set_style_overrides({.text=core::Color{16,46,77,255},.draw_background=false});
        label.set_vertical_align(ui::TextVerticalAlign::Center);label.set_text_fit_mode(ui::UiLabelTextFitMode::ShrinkToFit);
        label.set_padding(4);
    }
    set_values(0,0,0);
}
void RunStats::set_values(double seconds,int large_crystals,int small_crystals){
    _large=large_crystals;_small=small_crystals;
    _labels[0].set_text_content(ui::ui_raw_text(std::format("{:.1f} s",seconds)));
    _labels[1].set_text_content(ui::ui_raw_text(std::format("{}",large_crystals)));
    _labels[2].set_text_content(ui::ui_raw_text(std::format("{}",small_crystals)));
}
void RunStats::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const{
    if(!is_visible())return;
    const auto r=screen_rect();const float gap=8,cell=std::floor((r.width()-2*gap)/3);
    const bool compact=cell<160;
    const float scale=_grouped?r.height()/64:1.f;
    const float icon_center=(_grouped||compact?24.f:34.f)*scale,label_left=(_grouped||compact?44.f:62.f)*scale;
    // Portrait counters share one plate; give the timer more room than the counts.
    if(_grouped)hud_background(out,r);
    const std::array<float,4> edges{{0,r.width()*.4f,r.width()*.7f,r.width()}};
    const std::array<const char*,3> keys{{"fly.InGameUI.timeIcon","fly.InGameUI.uiCrystalLarge","fly.InGameUI.uiCrystalSmall"}};
    for(std::size_t i=0;i<3;++i){
        const core::Rect plate=_grouped?core::Rect{r.x()+edges[i],r.y(),edges[i+1]-edges[i],r.height()}
            :core::Rect{r.x()+(cell+gap)*i,r.y(),cell,r.height()};
        if(!_grouped)hud_background(out,plate);
        auto* icon=ELYSIA_RESOURCES->find_texture(keys[i]);float width=0,height=0;
        if(icon&&SDL_GetTextureSize(icon,&width,&height)){
            const float icon_height=_grouped?(i==2?22.f:32.f)*scale:std::min(r.height()*(i==2?.47f:.65f),compact?32.f:r.height()),icon_width=icon_height*width/height;
            image(out,icon,{plate.x()+icon_center-icon_width/2,plate.center().y-icon_height/2,icon_width,icon_height});
        }
        _labels[i].set_screen_rect({plate.x()+label_left,plate.y()+4*scale,plate.width()-label_left-8*scale,plate.height()-8*scale});_labels[i].submit_ui_render_commands(out);
    }
}
PauseIconButton::PauseIconButton(core::Rect rect,std::function<void()> action):UiButton(rect,25){
    set_style_overrides({.chrome={.draw_background=false,.draw_border=false}});
    set_state_textures({.idle=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnUp"),.pushed=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnDown")});
    set_on_click(std::move(action));
}
void PauseIconButton::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    UiButton::submit_ui_render_commands(out);const auto r=screen_rect();
    auto* text=ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnText");float width=0,height=0;
    if(!text||!SDL_GetTextureSize(text,&width,&height))return;
    const float extent=std::min(r.width(),r.height())*.5f,icon_width=extent*width/height;
    auto command=core::make_ui_texture_command(text,{r.center().x-icon_width/2,r.center().y-extent/2,icon_width,extent});
    apply_opacity(command);out.push_back(command);
}
MoonBackdrop::MoonBackdrop(const scene::SceneRuntimeContext& context,const char* name,int shade)
    :MoonBackdrop(core::Rect{0,0,static_cast<float>(context.logical_width()),static_cast<float>(context.logical_height())},name,shade){}
MoonBackdrop::MoonBackdrop(core::Rect viewport,const char* name,int shade)
    :UiElement(viewport,-100),_texture(texture(std::string("background_")+name)),_shade(shade){}
void MoonBackdrop::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    const auto rect=screen_rect();out.push_back(core::make_ui_fill_rect_command(rect,{4,13,36,255}));
    if(_texture){
        float width=0,height=0;SDL_GetTextureSize(_texture,&width,&height);
        if(width>0&&height>0){
            const float scale=std::max(rect.width()/width,rect.height()/height);
            auto command=core::make_ui_texture_command(_texture,rect);
            command.use_src_rect=true;command.src_rect={(width-rect.width()/scale)/2,(height-rect.height()/scale)/2,rect.width()/scale,rect.height()/scale};
            out.push_back(command);
        }
    }
    if(_shade)out.push_back(core::make_ui_fill_rect_command(rect,{3,13,37,static_cast<Uint8>(_shade)}));
    // A soft bottom vignette keeps navigation legible over the bright horizon.
    for(int step=0;step<24;++step)out.push_back(core::make_ui_fill_rect_command({rect.x(),rect.y()+rect.height()-144+step*6,rect.width(),6},{2,9,28,static_cast<Uint8>(step*4)}));
}
GlassButton::GlassButton(std::string text,float width,float height,std::function<void()> action):UiButton({0,0,width,height}){
    set_sounds({.click="fly.voice_6_3"});
    set_text_content(ui::ui_raw_text(std::move(text)));set_on_click(std::move(action));
    set_typography_role(typography::UiTypographyRole::ButtonCompact);
    set_style_overrides(glass_button_style());
}
void GlassButton::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    draw_glass_button(*this,out);
}
std::unique_ptr<ui::UiLabel> label(std::string text,float width,float height,bool centered){
    auto result=std::make_unique<ui::UiLabel>(core::Rect{0,0,width,height},0,ui::ui_raw_text(std::move(text)));
    result->set_typography_role(typography::UiTypographyRole::ButtonCompact);
    result->set_style_overrides({.text=core::Color{204,230,246,255}});
    result->set_horizontal_align(centered?ui::TextHorizontalAlign::Center:ui::TextHorizontalAlign::Left);
    result->set_vertical_align(ui::TextVerticalAlign::Center);return result;
}
void mark_selected(ui::UiButton& button){
    auto style=button.style_overrides();
    style.chrome.background.idle=core::Color{56,132,187,180};
    style.chrome.border.idle=core::Color{189,241,255,230};
    // Overrides replace the full record; keep geometry and interaction colors.
    button.set_style_overrides(style);
}
LevelNode::LevelNode(int id,int stars,bool open,std::function<void()> action,std::optional<int> best_score):UiButton({0,0,210,94}),_color(level_color(id)),_stars(stars),_open(open),_best_score(best_score){
    set_sounds({.click="fly.voice_6_3"});
    set_text_content(ui::ui_raw_text(std::format("{:02}",id)));set_on_click(std::move(action));set_enabled(open);
    set_typography_role(typography::UiTypographyRole::ButtonCompact);
    set_style_overrides({.chrome={.background={.idle=core::Color{5,25,64,70},.focused=core::Color{52,144,219,135},.disabled=core::Color{4,15,36,115}},
        .border={.idle=core::Color{133,209,255,30},.focused=core::Color{193,246,255,255},.disabled=core::Color{108,164,195,25}},.corner_radius=20.f},
        .text={.enabled=core::Color{241,253,255,255},.disabled=core::Color{146,166,191,255}}});
    _score_label.set_text_content(ui::ui_raw_text(content::format("level_best_score",
        _best_score?std::to_string(*_best_score):"--")));
    _score_label.set_typography_role(typography::UiTypographyRole::ButtonCompact);
    _score_label.set_horizontal_align(ui::TextHorizontalAlign::Center);
    _score_label.set_vertical_align(ui::TextVerticalAlign::Center);
    _score_label.set_style_overrides({.text=_open&&_best_score
        ?core::Color{255,224,156,255}:core::Color{146,166,191,255}});
}
void LevelNode::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    const auto r=screen_rect();
    UiButton::submit_ui_render_commands(out);
    if(_unlock_time>0)out.push_back(core::make_ui_fill_rect_command(r,{255,225,130,static_cast<Uint8>(100*_unlock_time/.6)},style().chrome.corner_radius));
    const auto& color=_color;
    image(out,texture("point"+color),{r.x()+16,r.y()+30,40,40},_open?255:65);
    if(!_open){
        // levelBtnLock is the original marker plate; lock<Color> is the standalone padlock.
        auto* lock=texture("lock"+color);float width=0,height=0;
        if(lock&&SDL_GetTextureSize(lock,&width,&height)&&width>0&&height>0){
            constexpr float icon_height=28.f;
            const float icon_width=icon_height*width/height;
            image(out,lock,{r.right()-20-icon_width,r.center().y-icon_height/2,icon_width,icon_height},160);
        }
    }
    for(int star=0;star<3;++star){
        // Original LevelSelectButtonV2: starDown is full, starUp is empty.
        const char* state=!_open||_stars<0?"starGrey":star<_stars?"starDown":"starUp";
        image(out,texture(std::string(state)+color),{r.center().x-34+star*24,r.y()+7,22,22},_open?255:85);
    }
    _score_label.set_screen_rect({r.x()+12,r.y()+70,std::max(0.f,r.width()-24),18});
    _score_label.submit_ui_render_commands(out);
}
ConstellationGrid::ConstellationGrid(const core::Rect& rect):UiGridContainer(rect){}
bool ConstellationGrid::on_ui_input_event(const ui::UiInputEvent& event){
    const auto inside=[&](const ui::UiElement& element){const auto r=element.screen_rect();return event.mouse_x>=r.x()&&event.mouse_x<r.right()&&event.mouse_y>=r.y()&&event.mouse_y<r.bottom();};
    if(event.type==ui::UiInputEventType::PointerPressed&&event.control==input::RawInputControl::MouseLeft){
        _locked_pressed=nullptr;
        for(std::size_t i=0;i<child_count();++i)if(auto* node=dynamic_cast<LevelNode*>(child_at(i));node&&!node->unlocked()&&node->is_visible()&&inside(*node)){
            _locked_pressed=node;return true;
        }
    }
    if(event.type==ui::UiInputEventType::PointerReleased&&_locked_pressed){
        auto* node=_locked_pressed;_locked_pressed=nullptr;if(inside(*node))game::audio::ui_click(false);return true;
    }
    return UiGridContainer::on_ui_input_event(event);
}
void ConstellationGrid::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    // Use the actual laid-out nodes, so map lines follow logical-size changes.
    for(std::size_t index=1;index<child_count();++index){
        const auto* previous=child_at(index-1);const auto* current=child_at(index);
        if(index%column_count()==0)continue;
        const auto a=previous->screen_rect(),b=current->screen_rect();
        out.push_back(core::make_ui_draw_line_command({a.right(),a.center().y},{b.x(),b.center().y},{162,224,255,160}));
    }
    UiGridContainer::submit_ui_render_commands(out);
}
AchievementCard::AchievementCard(int id,std::string title,std::string description,bool earned):UiElement({0,0,372,178}),
    _badge(texture(std::format("achv{}",id))),_earned(earned){
    _compact_title.set_text_content(ui::ui_raw_text(title));_compact_title.set_typography_role(typography::UiTypographyRole::Label);
    _compact_title.set_style_overrides({.text=core::Color{239,249,255,255}});
    _title.set_text_content(ui::ui_raw_text(std::move(title)));_title.set_typography_role(typography::UiTypographyRole::Label);
    _title.set_style_overrides({.text=core::Color{239,249,255,255}});_title.set_vertical_align(ui::TextVerticalAlign::Center);
    // Give the text wrapper a word boundary before numeric requirements so
    // values such as 100 / 10000 stay together in the original Chinese text.
    for(std::size_t index=1;index<description.size();++index){
        const auto current=static_cast<unsigned char>(description[index]),previous=static_cast<unsigned char>(description[index-1]);
        if(std::isdigit(current)&&(previous>=128||std::isalpha(previous))){description.insert(index," ");++index;}
    }
    _description.set_text_content(ui::ui_raw_text(std::move(description)));_description.set_typography_role(typography::UiTypographyRole::DialogBody);
    _description.set_style_overrides({.text=core::Color{177,207,229,255}});
    _status.set_text_content(ui::ui_raw_text(content::ui(earned?"earned":"not_earned")));_status.set_typography_role(typography::UiTypographyRole::ButtonCompact);
    _status.set_horizontal_align(ui::TextHorizontalAlign::Left);
    _status.set_style_overrides({.text=earned?core::Color{153,243,221,255}:core::Color{139,165,190,255}});
}
void AchievementCard::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    const auto r=screen_rect();
    out.push_back(core::make_ui_fill_rect_command(r,_earned?core::Color{17,63,103,224}:core::Color{8,30,62,220},16));
    out.push_back(core::make_ui_draw_rect_command(r,_earned?core::Color{117,231,242,200}:core::Color{102,160,198,80},16));
    if(r.width()<340){
        const core::Rect badge{r.x()+16,r.y()+16,64,64};image(out,_badge,badge);
        if(!_earned)out.push_back(core::make_ui_fill_rect_command(badge,{4,12,31,170},12));
        else out.push_back(core::make_ui_draw_rect_command({badge.x()-3,badge.y()-3,70,70},{142,239,248,150},14));
        const auto text=[&](ui::UiTextBlock& block,core::Rect area,float scale){
            block.set_screen_rect({0,0,area.width()/scale,area.height()/scale});
            std::vector<core::UiRenderCommand> commands;block.submit_ui_render_commands(commands);
            float fit=scale;
            for(const auto& command:commands)fit=std::min({fit,area.width()/std::max(1.f,command.screen_rect.right()),area.height()/std::max(1.f,command.screen_rect.bottom())});
            for(auto command:commands){const auto rect=command.screen_rect;command.screen_rect={area.x()+rect.x()*fit,area.y()+rect.y()*fit,rect.width()*fit,rect.height()*fit};out.push_back(command);}
        };
        text(_compact_title,{r.x()+92,r.y()+16,r.width()-108,64},.8f);
        text(_description,{r.x()+16,r.y()+92,r.width()-32,r.height()-132},.7f);
        _status.set_screen_rect({r.x()+16,r.bottom()-35,r.width()-32,25});_status.submit_ui_render_commands(out);
        return;
    }
    const core::Rect badge{r.x()+18,r.y()+24,92,92};image(out,_badge,badge);
    if(!_earned)out.push_back(core::make_ui_fill_rect_command(badge,{4,12,31,170},12));
    else out.push_back(core::make_ui_draw_rect_command({badge.x()-3,badge.y()-3,98,98},{142,239,248,150},14));
    _title.set_screen_rect({r.x()+126,r.y()+19,r.width()-142,32});_title.submit_ui_render_commands(out);
    // Let the engine wrap text at its body size, then present it at 21 logical
    // pixels. Fit unusually long translations into the same description area.
    constexpr float scale=.7f;
    const core::Rect description{r.x()+126,r.y()+61,r.width()-144,76};
    _description.set_screen_rect({0,0,description.width()/scale,description.height()/scale});
    std::vector<core::UiRenderCommand> text;_description.submit_ui_render_commands(text);
    for(auto command:text){
        const float fit=std::min(scale,description.height()/std::max(1.f,command.screen_rect.height()));
        const auto t=command.screen_rect;
        command.screen_rect={description.x()+t.x()*fit,description.y()+t.y()*fit,t.width()*fit,t.height()*fit};out.push_back(command);
    }
    _status.set_screen_rect({r.x()+20,r.y()+143,r.width()-40,25});_status.submit_ui_render_commands(out);
}
}
