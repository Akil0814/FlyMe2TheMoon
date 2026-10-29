#pragma once
#include "engine/ui/core/ui_element.h"
#include "game/content/content.h"
#include <algorithm>
#include <optional>

namespace game::frontend {
// Original TutorialDiagram's 10 Hz composition and recorded Demo pointer.
class TutorialUi final:public elysia::ui::UiElement {
public:
    explicit TutorialUi(elysia::core::Rect viewport={0,0,1280,720});
    void set_type(int type){_type=type;_time=0;}
    int type()const{return _type;}
    int frame()const{return static_cast<int>(_time*10+1e-7)%(_type==1?20:80);}
    void advance(double delta){_time+=std::max(0.,delta);_demo_time+=std::max(0.,delta);}
    void set_demo(bool enabled){if(enabled!=_demo){_demo_time=0;_pressed=false;_pointer.reset();}_demo=enabled;}
    void set_pointer_pressed(bool pressed){_pressed=pressed;}
    void set_pointer_position(elysia::core::Vector2 point){_pointer=point;}
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out)const override;
private:
    int _type=0;
    double _time=0,_demo_time=0;
    bool _demo=false,_pressed=false;
    std::optional<elysia::core::Vector2> _pointer;
    content::Json _frames;
};
}
