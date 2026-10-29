#pragma once
#include <cstdint>
#include <SDL3/SDL.h>

namespace game::platform {
inline std::uint64_t web_pause_generation=0;
inline void request_web_pause(){++web_pause_generation;}
inline void cancel_web_pointer(){
    if(!SDL_WasInit(SDL_INIT_VIDEO))return;
    SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.button=SDL_BUTTON_LEFT;event.button.which=SDL_TOUCH_MOUSEID;
    SDL_PushEvent(&event);
    event={};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;SDL_PushEvent(&event);
}
}
