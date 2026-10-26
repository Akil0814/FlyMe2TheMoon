#include "game/particles/particle_emitter.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace game::particles;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(float first,float last){return std::abs(first-last)<.001f;}
void check_emission(){
    EmitterConfig config;config.capacity=128;config.rate=100;config.life=1;config.life_variance=0;
    config.speed_variance=0;config.spread_degrees=0;config.end_size_variance=0;
    ParticleEmitter whole(config),split(config);const Emission emission{{10,20},{0,1}};
    whole.update(.1,emission);split.update(.05,emission);split.update(.05,emission);
    require(whole.particles().size()==10&&split.particles().size()==10,"Rate must accumulate births rather than emitting once per frame");
    for(std::size_t i=0;i<whole.particles().size();++i){
        const auto& a=whole.particles()[i];const auto& b=split.particles()[i];
        require(near(a.position.y,b.position.y)&&near(a.velocity.y,b.velocity.y)&&near(a.age,b.age),"Particle travel and age must agree across frame partitions");
        require(near(a.position.y,20+250*a.age+50*a.age*a.age),"Gravity must integrate analytically from each particle's birth time");
    }
    auto stopped=emission;stopped.enabled=false;
    const auto before=whole.particles().size();whole.update(.1,stopped);
    require(whole.particles().size()==before,"Stopping emission must not create or discard live generic particles");
    whole.update(2,stopped);require(whole.particles().empty(),"Stopped particles must expire");
    split.clear();split.update(.01,{{500,600},{0,1}});
    require(split.particles().size()==1&&near(split.particles().front().position.x,500),"Clear must reset old emitter positions before a teleport or restart");
}
void check_motion_and_capacity(){
    EmitterConfig config;config.rate=100;config.speed=0;config.speed_variance=0;config.gravity={};
    config.life=1;config.life_variance=0;
    ParticleEmitter moving(config);moving.update(.01,{{0,0},{0,1}});
    moving.update(.1,{{100,0},{0,1}});
    require(near(moving.particles().front().position.x,0)&&near(moving.particles().back().position.x,100),"Old particles must stay in the world while new emission follows the role");
    require(near(moving.particles()[1].position.x,10),"Emission must interpolate movement instead of clumping all births at the final position");
    ParticleEmitter bounded;
    for(int i=0;i<600;++i){bounded.update(1./120.,{});require(bounded.particles().size()<=60,"Original thrust pool must stay bounded");}
    bounded.update(10000,{});require(bounded.particles().size()<=60,"A long frame must have bounded catch-up work");
    const auto before=bounded.particles().size();bounded.update(-1,{});bounded.update(0,{});
    require(bounded.particles().size()==before,"Nonpositive frame durations must not advance particles");
    ParticleEmitter first({},42),second({},42);first.update(.1,{});second.update(.1,{});
    require(first.particles().size()==second.particles().size(),"Equal seeds must reproduce the birth count");
    for(std::size_t i=0;i<first.particles().size();++i)require((first.particles()[i].position-second.particles()[i].position).length()<.001f,"Equal seeds must reproduce spread and life variance");
}
}
int main(){try{check_emission();check_motion_and_capacity();std::cout<<"PASS: emission timing, gravity, frame partitions, world positions, lifetime, capacity and deterministic spread\n";return 0;}
catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
