#include "idle_cursor.h"

namespace game::platform {
CursorSample IdleCursor::sample_window(SDL_Window* window){
    CursorSample sample;sample.now=SDL_GetTicksNS();
    SDL_GetGlobalMouseState(&sample.x,&sample.y);
    float local_x=0,local_y=0;int width=0,height=0;
    SDL_GetMouseState(&local_x,&local_y);SDL_GetWindowSize(window,&width,&height);
    sample.focused=SDL_GetMouseFocus()==window&&SDL_GetKeyboardFocus()==window&&
        !(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)&&
        local_x>=0&&local_y>=0&&local_x<width&&local_y<height;
    return sample;
}
void IdleCursor::start(SDL_Window* window){
    stop();if(!window)return;
    if(_active)_active->stop();
    _active=this;_window=window;
    _hidden=!SDL_CursorVisible();set_hidden(false);
    _previous=_sampler(_window);_last_motion=_previous.now;
}
void IdleCursor::update(){
    if(!_window)return;
    const auto sample=_sampler(_window);
    if(!sample.focused||!_previous.focused||sample.x!=_previous.x||sample.y!=_previous.y||sample.now<_last_motion){
        _last_motion=sample.now;set_hidden(false);
    }else set_hidden(sample.now-_last_motion>=kIdleDuration);
    _previous=sample;
}
void IdleCursor::set_hidden(bool hidden)noexcept{
    if(hidden==_hidden)return;
    if(hidden?SDL_HideCursor():SDL_ShowCursor())_hidden=hidden;
}
void IdleCursor::stop()noexcept{
    if(_active==this){set_hidden(false);_active=nullptr;}
    _window=nullptr;_previous={};_last_motion=0;
}
}
