#pragma once
#include "engine/ui/core/ui_element.h"

namespace game::frontend {
// HUDLayer.showStarEatenEffectAtScreenPosInView: uses screen-space sprites.
class StarPickupEffects final:public elysia::ui::UiElement {
public:
    StarPickupEffects();
    void launch(elysia::core::Vector2 from,elysia::core::Vector2 destination);
    void advance(double delta);
    void clear(){_flights.clear();}
    int pending_count()const{return static_cast<int>(_flights.size());}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    struct Flight {elysia::core::Vector2 from,destination;double elapsed=0;};
    std::vector<Flight> _flights;
    SDL_Texture* _texture=nullptr;
    elysia::core::Vector2 _size;
};
}
