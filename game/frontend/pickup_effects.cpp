#include "pickup_effects.h"
#include "engine/resources/resource_service.h"
#include <algorithm>
#include <stdexcept>

namespace game::frontend {
using namespace elysia;
StarPickupEffects::StarPickupEffects():UiElement(core::Rect{0,0,1280,720},12){
    _texture=ELYSIA_RESOURCES->find_texture("fly.InGameUI.animateStar");
    float width=0,height=0;
    if(!_texture||!SDL_GetTextureSize(_texture,&width,&height))throw std::runtime_error("Missing original star pickup texture");
    _size={width*.5f,height*.5f};
    _flights.reserve(3);
}
void StarPickupEffects::launch(core::Vector2 from,core::Vector2 destination){
    _flights.push_back({from,destination});
}
void StarPickupEffects::advance(double delta){
    for(auto& flight:_flights)flight.elapsed+=std::max(0.,delta);
    // Summed frame deltas can fall a few ulps below the exact arrival time.
    std::erase_if(_flights,[](const auto& flight){return flight.elapsed>=.65-1e-9;});
}
void StarPickupEffects::submit_ui_render_commands(std::vector<core::UiRenderCommand>& out)const{
    if(!is_visible())return;
    for(const auto& flight:_flights){
        // 0.05 s ScaleTo(1.4), then 0.6 s Spawn(ScaleTo(0.6), MoveTo(slot)).
        const float grow=static_cast<float>(std::clamp(flight.elapsed/.05,0.,1.));
        const float travel=static_cast<float>(std::clamp((flight.elapsed-.05)/.6,0.,1.));
        const float scale=flight.elapsed<.05?1+.4f*grow:1.4f-.8f*travel;
        const auto position=flight.from+(flight.destination-flight.from)*travel;
        out.push_back(core::make_ui_texture_command(_texture,core::Rect::from_center(position,_size*scale)));
    }
}
}
