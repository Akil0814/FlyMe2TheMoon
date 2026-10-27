#pragma once
#include <SDL3/SDL.h>

namespace game::platform {
struct CursorSample {
    Uint64 now=0;
    float x=0,y=0;
    bool focused=false;
};

// Sampled independently of routed input and simulation time, including modal UI.
class IdleCursor final {
public:
    using Sampler=CursorSample(*)(SDL_Window*);
    explicit IdleCursor(Sampler sampler=nullptr):_sampler(sampler?sampler:sample_window){}
    ~IdleCursor(){stop();}
    IdleCursor(const IdleCursor&)=delete;
    IdleCursor& operator=(const IdleCursor&)=delete;
    void start(SDL_Window* window);
    void update();
    void stop()noexcept;
    bool hidden()const noexcept{return _hidden;}
    static bool hides_cursor()noexcept{return _active&&_active->_hidden;}
private:
    static CursorSample sample_window(SDL_Window* window);
    void set_hidden(bool hidden)noexcept;
    inline static IdleCursor* _active=nullptr;
    static constexpr Uint64 kIdleDuration=2'000'000'000;
    SDL_Window* _window=nullptr;
    Sampler _sampler;
    CursorSample _previous;
    Uint64 _last_motion=0;
    bool _hidden=false;
};
}
