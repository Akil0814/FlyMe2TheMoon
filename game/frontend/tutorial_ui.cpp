#include "tutorial_ui.h"
#include "engine/resources/resource_service.h"
#include <cmath>

namespace game::frontend {
using namespace elysia;
TutorialUi::TutorialUi():UiElement({0,0,1280,720},19),_frames(content::read("tutorial_visual.json").at("direction_frames")){}
void TutorialUi::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const {
    if(!is_visible())return;
    const auto draw=[&](const std::string& key,core::Vector2 point,float scale,core::Vector2 origin,double rotation=0){
        auto* texture=ELYSIA_RESOURCES->find_texture(key);float w=0,h=0;
        if(!texture||!SDL_GetTextureSize(texture,&w,&h))return;
        w*=scale;h*=scale;
        auto command=core::make_ui_texture_command(texture,{point.x-origin.x*w,point.y-origin.y*h,w,h});
        command.rotation_origin=origin;command.rotation_degrees=rotation;out.push_back(command);
    };
    if(_demo){
        // Despite its name, inGame/blackEdge.png is the localized "Demo" label.
        const auto begin=out.size();
        const auto language=content::language()=="zh-Hans"?std::string("zh"):content::language();
        draw("fly.frontend.tutorial_demo_"+language,{640,124},.75f,{.5f,0});
        if(out.size()>begin)out.back().alpha=static_cast<Uint8>(127+128*(1-std::abs(std::fmod(_demo_time,2.)-1)));
        // Keep both states above the dialogue, independent of camera/recorded coordinates.
        draw(_pressed?"fly.InGameUI.fingerDown":"fly.InGameUI.fingerUp",{640,420},.5f,{.5f,.5f});
    }
    if(!_type)return;
    // Preserve the original 320x480 local layout and source aspect ratios.
    // Only its placement/overall scale change to fit above the desktop dialogue.
    constexpr float scale=1.05f;
    const auto point=[](float x,float y){return core::Vector2{900+(x-17)*scale,132+(475-y)*scale};};
    const auto sprite=[&](const char* key,float x,float y,double rotation=0,core::Vector2 anchor={0,0}){
        draw(std::string("fly.TutorialUI.")+key,point(x,y),.5f*scale,anchor,rotation);
    };
    sprite("tutBubble",17,475);
    const char* doll="tutCharStand";
    if(_type==1){
        const bool pressed=frame()>=10;
        sprite(pressed?"tutDown":"tutUp",pressed?131:141,pressed?321:308.5f);
        if(pressed)sprite("tutArrow",160,352.8f,0,{.5f,.5f});
        doll=pressed?"tutCharUp":"tutCharStand";
    }else{
        const auto& f=_frames.at(frame());
        const auto moving=[&](const char* key,const content::Json& position){
            if(position[0]!=0||position[1]!=0)sprite(key,position[0],position[1]);
        };
        moving("tutUp",f.at("up"));moving("tutDown",f.at("down"));moving("tutBlank",f.at("blank"));
        const auto& arrow=f.at("arrow");
        if(arrow[0]!=0||arrow[1]!=0)sprite("tutArrow",160,352.8f,arrow[2],{.5f,.5f});
        // Native switch targets: 0=stand, 1=left, 2=right, 3=fall, 4=up.
        constexpr const char* dolls[]{"tutCharStand","tutCharLeft","tutCharRight","tutCharFall","tutCharUp"};
        doll=dolls[std::clamp(f.at("doll").get<int>(),0,4)];
    }
    sprite(doll,140.8f,386.1f);
}
}
