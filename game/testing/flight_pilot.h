#pragma once
#include "game/content/content.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace game::testing {
// QA only: a feedback controller translating route points into normal SDL input.
// It cannot access scene objects, physics state setters, progress, or story APIs.
class FlightPilot {
public:
    size_t waypoint=0;
    void update(SDL_WindowID window,double time,const content::Json& route,const content::Json& state) {
        if(_enter_down&&time-_enter_time>.09){key(window,false);_enter_down=false;}
        const bool finished=state.value("finished",false),dialogue=!state.value("dialogue",std::string()).empty();
        if(state.value("deaths",0)!=_deaths){_deaths=state.at("deaths");waypoint=0;}
        if(dialogue&&!finished&&time-_enter_time>.7){key(window,true);_enter_down=true;_enter_time=time;}
        if(finished||dialogue||state.value("paused",false)||!state.value("input_enabled",false)||state.value("demo",false)||!state.contains("pilot_view")) {_control_active=false;_zero_thrust_since=-1;button(window,false,0,0);return;}
        const float x=state.at("x"),y=state.at("y"),vx=state.at("vx"),vy=state.at("vy");
        if(!_control_active||state.value("portal_transits",0)!=_portals){
            _control_active=true;
            _portals=state.at("portal_transits");
            // A portal may trigger before the previous point's arrival radius.
            // Intro scripts may also move the role while input is disabled.
            // Resume at the nearest remaining waypoint using observed position.
            size_t nearest=waypoint;float distance=INFINITY;
            for(size_t i=waypoint;i<route.size();++i){const float d=std::hypot(route[i][0].get<float>()-x,route[i][1].get<float>()-y);if(d<distance){distance=d;nearest=i;}}
            waypoint=nearest;
        }
        // Focus/UI capture can require a neutral input frame before accepting a
        // held button again. Release/repress through SDL; never override routing.
        if(_mouse_down&&std::hypot(state.value("thrust_x",0.f),state.value("thrust_y",0.f))<.001f){
            if(_zero_thrust_since<0)_zero_thrust_since=time;
            if(time-_zero_thrust_since>.25){button(window,false,0,0);_release_until=time+.05;_zero_thrust_since=-1;return;}
        }else _zero_thrust_since=-1;
        if(time<_release_until)return;
        while(waypoint+1<route.size()&&std::hypot(route[waypoint][0].get<float>()-x,route[waypoint][1].get<float>()-y)<26)++waypoint;
        const float dx=route[waypoint][0].get<float>()-x,dy=route[waypoint][1].get<float>()-y;
        const float ax=(std::clamp(dx*1.5f,-105.f,105.f)-vx)*4+vx*1.1f-state.value("wind_x",0.f);
        const float ay=(std::clamp(dy*1.5f,-130.f,130.f)-vy)*4-320-state.value("wind_y",0.f);
        if(ay>=0){button(window,false,0,0);return;}
        constexpr float range=50*std::numbers::pi_v<float>/180;
        const float angle=std::clamp(std::atan2(ax,-ay),-range,range);
        const float input_angle=std::copysign(std::pow(std::abs(angle)/range,1/1.6f)*range,angle);
        const float magnitude=std::clamp(std::hypot(ax,ay)/896,0.f,1.f),distance=51.2f+12.8f*magnitude;
        // x/y identify the visual center; the original body anchor is 13 units below it.
        const float world_x=x-std::sin(input_angle)*distance,world_y=y+13+std::cos(input_angle)*distance;
        const auto& view=state.at("pilot_view");const float zoom=view[2];
        const float logical_x=(world_x-view[0].get<float>())*zoom,logical_y=(world_y-view[1].get<float>())*zoom;
        float mouse_x=logical_x,mouse_y=logical_y;
        // SDL input uses window coordinates; the engine converts to logical units.
        // Preserve that round trip when the user resizes the game or uses DPI scaling.
        if(auto* renderer=SDL_GetRenderer(SDL_GetWindowFromID(window)))
            if(!SDL_RenderCoordinatesToWindow(renderer,logical_x,logical_y,&mouse_x,&mouse_y))throw std::runtime_error("QA pointer coordinate conversion failed");
        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=window;motion.motion.x=mouse_x;motion.motion.y=mouse_y;
        SDL_PushEvent(&motion);button(window,true,mouse_x,mouse_y);
    }
private:
    bool _mouse_down=false,_enter_down=false,_control_active=false;
    int _deaths=0,_portals=0;
    double _enter_time=-1;
    double _zero_thrust_since=-1,_release_until=-1;
    void key(SDL_WindowID window,bool down){SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.windowID=window;e.key.scancode=SDL_SCANCODE_RETURN;e.key.key=SDLK_RETURN;e.key.down=down;SDL_PushEvent(&e);}
    void button(SDL_WindowID window,bool down,float x,float y){if(down==_mouse_down)return;_mouse_down=down;SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.windowID=window;e.button.button=SDL_BUTTON_LEFT;e.button.down=down;e.button.x=x;e.button.y=y;SDL_PushEvent(&e);}
};
}
