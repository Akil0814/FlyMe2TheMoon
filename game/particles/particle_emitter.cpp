#include "particle_emitter.h"
#include <numbers>

namespace game::particles {
void ParticleEmitter::advance(Particle& particle,double delta)const{
    const float dt=static_cast<float>(delta);
    particle.position+=particle.velocity*dt+_config.gravity*(.5f*dt*dt);
    particle.velocity+=_config.gravity*dt;particle.age+=delta;
}
void ParticleEmitter::update(double delta,const Emission& emission){
    if(!std::isfinite(delta)||delta<=0)return;
    for(auto& particle:_particles)advance(particle,delta);
    std::erase_if(_particles,[](const auto& particle){return particle.age>=particle.life;});
    if(!emission.enabled||_config.rate<=0||_config.capacity==0){_remainder=0;_previous_position.reset();return;}
    const double interval=1/_config.rate;
    const double total=_remainder+delta;
    const double count=std::floor(total/interval+1e-9);
    _remainder=std::max(0.,total-count*interval);
    // Only the most recent capacity births can matter after a long frame.
    const auto births=static_cast<std::size_t>(std::min(count,static_cast<double>(_config.capacity)));
    const Vec previous=_previous_position.value_or(emission.position);
    const float direction=std::atan2(emission.direction.y,emission.direction.x);
    for(std::size_t i=0;i<births&&_particles.size()<_config.capacity;++i){
        const double age=_remainder+(births-i-1)*interval;
        const double life=std::max(.001,_config.life+(unit()*2-1)*_config.life_variance);
        const float angle=direction+(unit()*2-1)*_config.spread_degrees*std::numbers::pi_v<float>/180;
        const float speed=std::max(0.f,_config.speed+(unit()*2-1)*_config.speed_variance);
        const float end_size=std::max(0.f,emission.end_size+(unit()*2-1)*_config.end_size_variance);
        if(age>=life)continue;
        const Vec origin=previous+(emission.position-previous)*static_cast<float>(std::clamp((delta-age)/delta,0.,1.));
        Particle particle{origin,{std::cos(angle)*speed,std::sin(angle)*speed},0,life,emission.start_size,end_size};
        advance(particle,age);_particles.push_back(particle);
    }
    _previous_position=emission.position;
}
void ParticleEmitter::submit(std::vector<elysia::core::RenderCommand>& out,SDL_Texture* texture)const{
    if(!texture)return;
    const auto blend=[](std::uint8_t first,std::uint8_t last,float t){return static_cast<std::uint8_t>(std::clamp(first+(last-first)*t,0.f,255.f));};
    for(const auto& particle:_particles){
        const float t=static_cast<float>(std::clamp(particle.age/particle.life,0.,1.));
        const float size=std::max(0.f,particle.start_size+(particle.end_size-particle.start_size)*t);
        if(size<=0)continue;
        elysia::core::RenderCommand command;command.texture=texture;
        command.command_rect={particle.position.x-size/2,particle.position.y-size/2,size,size};
        command.alpha=blend(_config.start_color.a,_config.end_color.a,t);
        command.texture_color_modulation=elysia::core::TextureColorModulation{
            blend(_config.start_color.r,_config.end_color.r,t),blend(_config.start_color.g,_config.end_color.g,t),blend(_config.start_color.b,_config.end_color.b,t)};
        out.push_back(command);
    }
}
}
