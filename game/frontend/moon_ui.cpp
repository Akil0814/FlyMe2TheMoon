#include "moon_ui.h"
#include "engine/resources/resource_service.h"
#include "game/content/content.h"
#include <format>
#include <cmath>
#include <cctype>

namespace game::frontend {
using namespace elysia;
namespace {
SDL_Texture* texture(const std::string& key){return ELYSIA_RESOURCES->find_texture("fly.frontend."+key);}
void image(std::vector<core::UiRenderCommand>& out,SDL_Texture* source,core::Rect rect,Uint8 alpha=255){
    if(!source)return;
    auto command=core::make_ui_texture_command(source,rect);command.alpha=alpha;out.push_back(command);
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
void StarRating::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    const auto r=screen_rect();const float gap=r.height()*.1875f;
    const float side=std::min(r.height(),(r.width()-gap*2)/3);
    const float x=r.center().x-(side*3+gap*2)/2,y=r.center().y-side/2;
    for(int index=0;index<3;++index){
        auto* star=ELYSIA_RESOURCES->find_texture(index<_collected?"fly.InGameUI.starDown":"fly.frontend.starGreyY");
        image(out,star,{x+(side+gap)*index,y,side,side});
    }
}
void MoonPanel::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    const auto r=screen_rect();
    out.push_back(core::make_ui_fill_rect_command(r,{9,30,62,242},22));
    out.push_back(core::make_ui_draw_rect_command(r,{124,211,247,170},22));
    out.push_back(core::make_ui_draw_line_command({r.x()+32,r.y()+12},{r.right()-32,r.y()+12},{156,233,255,80}));
}
PauseIconButton::PauseIconButton(core::Rect rect,std::function<void()> action):UiButton(rect,12){
    set_style_overrides(glass_button_style());set_on_click(std::move(action));
}
void PauseIconButton::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    draw_glass_button(*this,out);const auto r=screen_rect();
    image(out,ELYSIA_RESOURCES->find_texture("fly.InGameUI.pauseBtnText"),{r.center().x-11,r.center().y-14,22,28});
}
MenuArtwork::MenuArtwork():UiElement({0,0,544,816},-90),
    _background(ELYSIA_RESOURCES->find_texture("fly.ui.mainBg2")),_title(ELYSIA_RESOURCES->find_texture("fly.ui.mainTitle2")){}
void MenuArtwork::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!_background||!_title)return;
    const auto r=screen_rect();const float scale=r.height()/960;
    // Original 640x276 title is the background crop beginning at Y=539,
    // including the character's boots. Replace that band, using the same scale
    // and source coordinates; drawing it as a separate banner breaks alignment.
    const auto column=[&](core::Rect clip,Uint8 alpha){
        const auto band=[&](SDL_Texture* source,float source_y,float height,float dest_y){
            auto command=core::make_ui_texture_command(source,{r.x(),r.y()+dest_y*scale,r.width(),height*scale});
            command.use_src_rect=true;command.src_rect={0,source_y,640,height};command.alpha=alpha;
            core::set_ui_command_clip_rect(command,clip);out.push_back(command);
        };
        band(_background,0,539,0);
        band(_title,0,276,539);
        band(_background,815,145,815);
    };
    // Clip full-width quads instead of subdividing texture geometry: fractional
    // strip widths can leave visible raster gaps across the character.
    constexpr float fade=40;
    column({r.x()+fade,0,r.width()-2*fade,720},255);
    for(int step=0;step<20;++step){
        const auto alpha=static_cast<Uint8>((step+.5f)*255/20);
        column({r.x()+step*2,0,2,720},alpha);
        column({r.right()-(step+1)*2,0,2,720},alpha);
    }
    for(int row=0;row<60;row+=2)out.push_back(core::make_ui_fill_rect_command({0,660.f+row,1280,2},{2,9,28,static_cast<Uint8>(row*255/60)}));
}
MoonBackdrop::MoonBackdrop(const char* name,int shade):UiElement({0,0,1280,720},-100),_texture(texture(std::string("background_")+name)),_shade(shade){}
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
LevelNode::LevelNode(int id,int chapter,int stars,bool open,std::function<void()> action,std::optional<int> best_score):UiButton({0,0,210,94}),_chapter(chapter),_stars(stars),_open(open),_best_score(best_score){
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
    const std::string color(1,"OGYP"[_chapter]);
    image(out,texture("point"+color),{r.x()+16,r.y()+30,40,40},_open?255:65);
    if(!_open)image(out,texture("levelBtnLock"),{r.x()+170,r.y()+39,20,25},160);
    for(int star=0;star<3;++star){
        // Original LevelSelectButtonV2: starDown is full, starUp is empty.
        const char* state=!_open||_stars<0?"starGrey":star<_stars?"starDown":"starUp";
        image(out,texture(std::string(state)+color),{r.center().x-34+star*24,r.y()+7,22,22},_open?255:85);
    }
    _score_label.set_screen_rect({r.x()+12,r.y()+70,std::max(0.f,r.width()-24),18});
    _score_label.submit_ui_render_commands(out);
}
ConstellationGrid::ConstellationGrid(const core::Rect& rect):UiGridContainer(rect){}
void ConstellationGrid::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    // Use the actual laid-out nodes, so map lines follow logical-size changes.
    for(std::size_t index=1;index<child_count();++index){
        const auto* previous=child_at(index-1);const auto* current=child_at(index);
        if(index%5==0)continue;
        const auto a=previous->screen_rect().center(),b=current->screen_rect().center();
        out.push_back(core::make_ui_draw_line_command({a.x+103,a.y},{b.x-103,b.y},{162,224,255,160}));
    }
    UiGridContainer::submit_ui_render_commands(out);
}
AchievementCard::AchievementCard(int id,std::string title,std::string description,bool earned):UiElement({0,0,372,178}),
    _badge(texture(std::format("achv{}",id))),_earned(earned){
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
    const auto r=screen_rect();
    out.push_back(core::make_ui_fill_rect_command(r,_earned?core::Color{17,63,103,224}:core::Color{8,30,62,220},16));
    out.push_back(core::make_ui_draw_rect_command(r,_earned?core::Color{117,231,242,200}:core::Color{102,160,198,80},16));
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
