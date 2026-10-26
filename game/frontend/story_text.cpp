#include "story_text.h"
#include "engine/localization/localization_service.h"
#include <charconv>
#include <cctype>

namespace game::frontend {
using namespace elysia;
void StoryText::set_story_text(const std::string& source){
    _glyphs.clear();_time=0;_revealed=false;auto color=style().text;std::string plain;
    for(std::size_t i=0;i<source.size();){
        if(source.compare(i,2,"#n")==0){color=style().text;i+=2;continue;}
        if(source.compare(i,2,"#c")==0&&i+8<=source.size()){
            unsigned rgb=0;const auto result=std::from_chars(source.data()+i+2,source.data()+i+8,rgb,16);
            if(result.ec==std::errc{}&&result.ptr==source.data()+i+8){color={static_cast<Uint8>(rgb>>16),static_cast<Uint8>(rgb>>8),static_cast<Uint8>(rgb),255};i+=8;continue;}
        }
        const auto lead=static_cast<unsigned char>(source[i]);
        const std::size_t count=lead<128?1:lead<224?2:lead<240?3:4;
        auto text=source.substr(i,std::min(count,source.size()-i));i+=text.size();plain+=text;_glyphs.push_back({std::move(text),color});
    }
    set_text_content(ui::ui_raw_text(plain));
}
void StoryText::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible()||_glyphs.empty())return;
    const auto r=screen_rect();const float inset=static_cast<float>(padding()),width=r.width()-2*inset;
    if(width<=0)return;
    localization::LocalizedTextStyle text_style{.typography_role=typography_role(),.font_source_override=font_source_override()};
    struct Placed {std::size_t index;float x,y,w,h;};std::vector<Placed> placed;
    int measured_width=0,line_height=0;
    if(!ELYSIA_LOCALIZATION->measure_raw_text("Mg",text_style,measured_width,line_height)||line_height<=0)return;
    float x=0,y=0;
    for(std::size_t index=0;index<_glyphs.size();++index){
        const auto& glyph=_glyphs[index];
        if(glyph.text=="\n"){x=0;y+=line_height;continue;}
        if(glyph.text=="\r")continue;
        // Keep ordinary Latin words together; CJK text naturally wraps by glyph.
        const auto ascii_word=[](const std::string& s){return s.size()==1&&(std::isalnum(static_cast<unsigned char>(s[0]))||s=="'");};
        if(ascii_word(glyph.text)&&(index==0||!ascii_word(_glyphs[index-1].text))){
            std::string word;
            for(auto j=index;j<_glyphs.size()&&ascii_word(_glyphs[j].text);++j)word+=_glyphs[j].text;
            int w=0,h=0;
            if(ELYSIA_LOCALIZATION->measure_raw_text(word,text_style,w,h)&&w<=width&&x+w>width){x=0;y+=line_height;}
        }
        int w=0,h=0;if(!ELYSIA_LOCALIZATION->measure_raw_text(glyph.text,text_style,w,h))continue;
        if(x+w>width&&x>0){x=0;y+=line_height;}
        placed.push_back({index,x,y,static_cast<float>(w),static_cast<float>(h)});x+=w;
    }
    const float scale=std::min(1.f,std::max(0.f,r.height()-2*inset)/(y+line_height));
    for(const auto& glyph:placed){
        if(glyph.index>=visible_count())break;
        text_style.color=_glyphs[glyph.index].color;
        auto* texture=ELYSIA_LOCALIZATION->get_raw_text_texture(_glyphs[glyph.index].text,text_style);
        if(!texture)continue;
        auto command=core::make_ui_texture_command(texture,{r.x()+inset+glyph.x*scale,r.y()+inset+glyph.y*scale,glyph.w*scale,glyph.h*scale});
        apply_opacity(command);out.push_back(command);
    }
}
}
