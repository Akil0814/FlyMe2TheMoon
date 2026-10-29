#pragma once
#include "engine/core/geometry/rect.h"
#include <algorithm>

namespace game::frontend {
struct FlightUiLayout {
    elysia::core::Rect viewport,stage,stars,stats,pause,dialogue,portrait,face,text,modal,modal_content;
    bool vertical=false;
    static FlightUiLayout make(int width,int height){
        using elysia::core::Rect;
        FlightUiLayout layout{};layout.viewport={0,0,static_cast<float>(width),static_cast<float>(height)};
        layout.vertical=height>=width;
        if(layout.vertical){
            // One compact row: stage, time, large/small crystals, stars, pause.
            const float scale=width/640.f;
            const auto hud=[&](Rect r){return Rect{r.x()*scale,r.y()*scale,r.width()*scale,r.height()*scale};};
            layout.stage=hud({12,12,128,48});layout.stats=hud({148,12,276,48});
            layout.stars=hud({432,16,132,40});layout.pause=hud({572,8,56,56});
            const float top=static_cast<float>(height)-272;
            layout.dialogue={16,top,static_cast<float>(width)-32,248};layout.portrait={28,top+16,112,112};
            layout.face={36,top+24,96,96};layout.text={156,top+16,static_cast<float>(width)-188,216};
        }else{
            const float scale=std::min(width/1280.f,height/720.f);
            const float x=(width-1280*scale)/2,y=(height-720*scale)/2;
            const auto map=[&](Rect r){return Rect{x+r.x()*scale,y+r.y()*scale,r.width()*scale,r.height()*scale};};
            layout.stage=map({16,12,280,64});layout.stars=map({982,24,136,40});layout.stats=map({312,12,620,64});layout.pause=map({1192,12,64,64});
            layout.dialogue=map({24,474,1232,182});layout.portrait=map({38,486,156,156});layout.face=map({46,494,140,140});layout.text=map({212,492,1024,144});
        }
        layout.modal={0,0,std::min(512.f,static_cast<float>(width)-48),560};
        layout.modal_content={0,0,layout.modal.width()-80,500};return layout;
    }
};
}
