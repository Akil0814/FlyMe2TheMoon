#pragma once
#include "engine/core/geometry/rect.h"
#include <algorithm>

namespace game::frontend {
struct FlightUiLayout {
    elysia::core::Rect viewport,stage,stars,stats,pause,dialogue,face,text,skip,modal,modal_content;
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
            // Keep the original bottom-anchored composition with a smaller portrait
            // and lower plate, leaving more width for the unchanged dialogue font.
            const float fit=std::min(width/640.f,height/960.f),bottom=static_cast<float>(height);
            layout.dialogue={0,bottom-270*fit,static_cast<float>(width),270*fit};
            layout.face={0,bottom-289*fit,227.8f*fit,289*fit};
            layout.text={220*fit,bottom-270*fit,width-236*fit,214*fit};
            layout.skip={width-145*fit,bottom-59*fit,145*fit,59*fit};
        }else{
            const float scale=std::min(width/1280.f,height/720.f);
            const float x=(width-1280*scale)/2,y=(height-720*scale)/2;
            const auto map=[&](Rect r){return Rect{x+r.x()*scale,y+r.y()*scale,r.width()*scale,r.height()*scale};};
            layout.stage=map({16,12,280,64});layout.stars=map({982,24,136,40});layout.stats=map({312,12,620,64});layout.pause=map({1192,12,64,64});
            layout.dialogue=map({0,510,1280,210});layout.face=map({16,488.8f,182.24f,231.2f});
            layout.text=map({216,512,1040,144});layout.skip=map({1135,661,145,59});
        }
        layout.modal={0,0,std::min(512.f,static_cast<float>(width)-48),560};
        layout.modal_content={0,0,layout.modal.width()-80,500};return layout;
    }
};
}
