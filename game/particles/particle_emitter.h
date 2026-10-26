#pragma once
#include "engine/core/render/render_command.h"
#include <span>

namespace game::particles {
using Vec=elysia::core::Vector2;
using Color=elysia::core::Color;
struct EmitterConfig {
    std::size_t capacity=60;
    double rate=300,life=.2,life_variance=.02;
    float speed=250,speed_variance=25,spread_degrees=9,end_size_variance=15;
    Vec gravity{0,100};
    Color start_color{128,158,182,255},end_color{128,158,182,0};
};
struct Emission {
    Vec position{},direction{0,1};
    float start_size=28,end_size=14;
    bool enabled=true;
};
struct Particle {
    Vec position{},velocity{};
    double age=0,life=.2;
    float start_size=28,end_size=14;
};
// Small CPU emitter owned by the game. Particles retain world-space positions.
class ParticleEmitter {
public:
    explicit ParticleEmitter(EmitterConfig config={},std::uint32_t seed=0x735e):_config(config),_random(seed){_particles.reserve(config.capacity);}
    void clear(){_particles.clear();_remainder=0;_previous_position.reset();}
    std::span<const Particle> particles()const{return _particles;}
    void update(double delta,const Emission& emission);
    void submit(std::vector<elysia::core::RenderCommand>& out,SDL_Texture* texture)const;
private:
    float unit(){_random=_random*1664525u+1013904223u;return (_random>>8)*(1.f/16777216);}
    void advance(Particle& particle,double delta)const;
    EmitterConfig _config;
    std::vector<Particle> _particles;
    std::uint32_t _random;
    double _remainder=0;
    std::optional<Vec> _previous_position;
};
}
