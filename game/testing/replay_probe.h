#pragma once
#include "game/content/content.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <set>
#include "flight_pilot.h"

namespace game::testing {
// Explicit opt-in QA: injects input events only. Never changes simulation positions or success rules.
class ReplayProbe {
public:
    ~ReplayProbe();
    static ReplayProbe& shared(){static ReplayProbe probe;return probe;}
    bool enabled()const{return _enabled;}
    void begin();
    bool update(SDL_Renderer* renderer,const content::Json& state);
    static int requested_level();
private:
    content::Json _scenario,_samples=content::Json::array();
    std::filesystem::path _output;
    Uint64 _start=0;
    size_t _next=0;
    double _last_sample=-1;
    double _success_at=-1;
    double _failure_at=-1;
    bool _enabled=false;
    std::set<SDL_Scancode> _pressed;
    FlightPilot _pilot;
};
}
