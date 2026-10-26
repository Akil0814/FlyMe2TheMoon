#pragma once
#include "laser_visual_poses.h"
#include "laser_geometry.h"
#include "engine/core/render/render_command.h"
#include "engine/resources/resource_service.h"
#include <stdexcept>

namespace game::play {
// Game-specific presentation. Collision queries remain in Entity::update_laser.
class LaserVisual {
    using Vec=elysia::core::Vector2;
    using Frame=elysia::resources::FrameInfo;
    using Command=elysia::core::RenderCommand;
    struct Particle {Vec position,velocity;double age=0,life=.2;float end_size=8;};
public:
    LaserVisual(const std::string& key,int seed):_random(static_cast<std::uint32_t>(seed)+0x1b69e0u) {
        _base=frame(key,0);
        for(int i=0;i<2;++i){_head[i]=frame(key+".S01",i);_beam[i]=frame(key+".S02",i);}
        for(int i=0;i<4;++i)_lightning[i]=frame(key+".S03",i);
        for(int i=0;i<20;++i)_impact[i]=frame(key+".S04",i);
        _star=ELYSIA_RESOURCES->find_texture("fly.laser_stars");
        if(!_star)throw std::runtime_error("Missing laser particle texture");
    }
    void update(double delta,bool active,bool hitting,Vec hit) {
        delta=std::max(0.,delta);
        for(auto& p:_particles){p.age+=delta;p.position+=p.velocity*static_cast<float>(delta)+Vec{0,static_cast<float>(50*delta*delta)};p.velocity.y+=static_cast<float>(100*delta);}
        std::erase_if(_particles,[](const auto& p){return p.age>=p.life;});
        if(active) {
            _time=std::fmod(_time+delta,20./60.);_lightning_time+=delta;
            while(_lightning_time>=1./12.){_lightning_time-=1./12.;_lightning_frame=(_lightning_frame+1+random()%3)%4;}
        }
        if(!active||!hitting){_emission=0;return;}
        // Original worldParticle: 36 particles, life .2 +/- .02, rate 180/s,
        // size 4 -> 8 +/- 8, speed 50 +/- 5, 270 +/- 90 Y-up, gravity -100.
        _emission+=delta*180;
        const int count=static_cast<int>(_emission);_emission-=count;
        for(int i=std::max(0,count-36);i<count;++i) {
            const double age=(count-i-1)/180.;const double life=.18+.04*unit();
            if(age>=life||_particles.size()>=36)continue;
            const float angle=static_cast<float>((unit()*180+180)*std::numbers::pi/180);
            const float speed=45+10*unit();const Vec velocity{speed*std::cos(angle),-speed*std::sin(angle)};
            _particles.push_back({hit+velocity*static_cast<float>(age)+Vec{0,static_cast<float>(50*age*age)},velocity+Vec{0,static_cast<float>(100*age)},age,life,16*unit()});
        }
    }
    void submit(std::vector<Command>& out,Vec center,Vec size,float rotation,float local_angle,bool active,float length,bool hitting)const {
        constexpr float degrees=180/std::numbers::pi_v<float>;
        const float body_angle=rotation/degrees;
        const int index=std::clamp(static_cast<int>((180.-local_angle*180./std::numbers::pi)/3.),0,60);
        const auto local=[&](Vec p){return center+rotate({p.x-size.x/2,size.y/2-p.y},body_angle);};
        const float beam_angle=body_angle-local_angle;
        const Vec direction{std::cos(beam_angle),std::sin(beam_angle)};
        const Vec start=center+rotate(laser_muzzle_offset(local_angle,size),body_angle);
        const Vec end=start+direction*length;
        const float beam_rotation=beam_angle*degrees+90;
        if(active&&length>0) {
            // S02/002 caps the first nine units, S02/001 stretches the shaft.
            append(out,*_beam[1],start,{_beam[1]->_width*.5f,std::min(9.f,length)},beam_rotation,{.5f,1});
            if(length>9) {
                append(out,*_beam[0],start+direction*9,{_beam[0]->_width*.5f,length-9},beam_rotation,{.5f,1});
                const auto& lightning=*_lightning[_lightning_frame];
                append(out,lightning,start+direction*9,{lightning._width*.5f,lightning._height*.5f*std::min((length-9)/53,1.f)},beam_rotation,{.5f,1});
            }
        }
        // Both S01 parts are children at z=-1, below the fixed base image.
        append(out,*_head[1],local(kLaserStemPoses[index].position),native_size(*_head[1]),rotation+kLaserStemPoses[index].rotation,{0,1});
        append(out,*_head[0],local(kLaserHeadPoses[index].position),native_size(*_head[0]),rotation+kLaserHeadPoses[index].rotation,{0,1});
        append(out,*_base,center,size,rotation,{.5f,.5f});
        if(active&&hitting)append(out,*_impact[static_cast<std::size_t>(_time*60+1e-7)%20],end,{32,32},beam_rotation,{.5f,.5f});
        for(const auto& p:_particles) {
            const float t=static_cast<float>(p.age/p.life),extent=4+(p.end_size-4)*t;
            Command c;c.texture=_star;c.command_rect={p.position.x-extent/2,p.position.y-extent/2,extent,extent};
            c.alpha=static_cast<std::uint8_t>(255*(1-t));out.push_back(c);
        }
    }
private:
    static Vec rotate(Vec p,float r){return {p.x*std::cos(r)-p.y*std::sin(r),p.x*std::sin(r)+p.y*std::cos(r)};}
    static Vec native_size(const Frame& f){return {f._width*.5f,f._height*.5f};}
    static const Frame* frame(const std::string& key,int index) {
        const auto* atlas=ELYSIA_RESOURCES->find_atlas(key);const auto* f=atlas?atlas->frame_at(index):nullptr;
        if(!f||!f->_texture)throw std::runtime_error("Missing laser sprite "+key);return f;
    }
    static void append(std::vector<Command>& out,const Frame& f,Vec anchor,Vec extent,float rotation,Vec origin) {
        Command c;c.texture=f._texture;c.command_rect={anchor.x-origin.x*extent.x,anchor.y-origin.y*extent.y,extent.x,extent.y};
        c.rotation_origin=origin;c.rotation_degrees=rotation;c.use_src_rect=f._source_rect.has_value();if(c.use_src_rect)c.src_rect=*f._source_rect;out.push_back(c);
    }
    std::uint32_t random(){_random=_random*1664525u+1013904223u;return _random;}
    float unit(){return (random()>>8)*(1.f/16777216);}
    const Frame* _base=nullptr;
    std::array<const Frame*,2> _head{},_beam{};
    std::array<const Frame*,4> _lightning{};
    std::array<const Frame*,20> _impact{};
    SDL_Texture* _star=nullptr;
    std::vector<Particle> _particles;
    std::uint32_t _random;
    double _time=0,_lightning_time=0,_emission=0;
    int _lightning_frame=0;
};
}
