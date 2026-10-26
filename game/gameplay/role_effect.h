#pragma once
#include "engine/core/geometry/vector2.h"
#include <algorithm>
#include <cmath>

namespace game::play {
enum class RoleEffect { None, Enter, Moon, PortalIn, Death };
enum class BallImage { None, Ball, Disappear, Explode };
struct RoleEffectPose {
    BallImage image=BallImage::None;
    int frame=0;
    float scale=1;
    bool body_visible=true;
};
// RoleViewBallEffect: 30 Hz, independent of the gameplay animation atlas rate.
inline RoleEffectPose role_effect_pose(RoleEffect effect,double seconds) {
    const int frame=static_cast<int>(std::max(0.,seconds)*30+1e-7);
    if(effect==RoleEffect::None)return {};
    if(effect==RoleEffect::Enter) {
        if(frame==0)return {BallImage::None,0,1,false};
        if(frame<=4)return {BallImage::Disappear,frame-1,1,false};
        if(frame<=9)return {BallImage::Ball,0,.5f+.1f*(frame-5),false};
        if(frame<=15)return {BallImage::Explode,frame-10,.9f,true};
        return {};
    }
    if(effect==RoleEffect::Death) {
        if(frame<30)return {};
        if(frame==30)return {BallImage::Ball,0,1,false};
        if(frame<37)return {BallImage::Explode,frame-31,1,false};
        return {BallImage::None,0,1,false};
    }
    const int shrink=effect==RoleEffect::Moon?26:10;
    if(frame<6)return {BallImage::Explode,5-frame,1,true};
    if(frame<6+shrink)return {BallImage::Ball,0,1.f-.5f*(frame-6)/shrink,false};
    if(frame<10+shrink)return {BallImage::Disappear,3-(frame-6-shrink),1,false};
    return {BallImage::None,0,1,false};
}

enum class RoleTransitionKind { None, Moon, Portal };
enum class RoleTransitionPhase { None, MoonApproach, MoonWait, PortalPull, PortalTravel, PortalEmerge, Complete };
struct RoleTransition {
    using Vec=elysia::core::Vector2;
    RoleTransitionKind kind=RoleTransitionKind::None;
    Vec from{},entrance{},destination{};
    double time=0,travel=0;
    static constexpr double kPull=2./3.,kEmerge=.3,kMoonMove=1.2,kMoonEnd=2.4;
    void begin_moon(Vec start,Vec moon){*this={RoleTransitionKind::Moon,start,{},moon};}
    void begin_portal(Vec start,Vec source,Vec target){
        *this={RoleTransitionKind::Portal,start,source,target};
        travel=std::pow((target-source).length()/1000.,.6);
    }
    bool active()const{return kind!=RoleTransitionKind::None;}
    void advance(double delta){time+=std::max(0.,delta);}
    RoleTransitionPhase phase()const {
        if(kind==RoleTransitionKind::None)return RoleTransitionPhase::None;
        if(kind==RoleTransitionKind::Moon)return time<kMoonMove?RoleTransitionPhase::MoonApproach:time<kMoonEnd?RoleTransitionPhase::MoonWait:RoleTransitionPhase::Complete;
        if(time<kPull)return RoleTransitionPhase::PortalPull;
        // The native callback includes a 0.138 s margin after the camera move.
        if(time<kPull+travel+.138)return RoleTransitionPhase::PortalTravel;
        if(time<kPull+travel+.138+16./30.)return RoleTransitionPhase::PortalEmerge;
        return RoleTransitionPhase::Complete;
    }
    double emerge_time()const{return std::max(0.,time-kPull-travel-.138);}
    Vec position()const {
        if(kind==RoleTransitionKind::Moon){const float t=static_cast<float>(std::clamp(time/kMoonMove,0.,1.));return from+(destination-from)*std::sqrt(t);}
        if(phase()==RoleTransitionPhase::PortalPull){const float t=static_cast<float>(std::clamp(time/kPull,0.,1.));return from+(entrance-from)*(t*t);}
        return phase()==RoleTransitionPhase::PortalTravel?entrance:destination;
    }
    float scale()const {
        if(phase()==RoleTransitionPhase::PortalPull){const float t=static_cast<float>(time/kPull);return 1-.5f*t*t;}
        if(phase()==RoleTransitionPhase::PortalEmerge){const float t=static_cast<float>(std::clamp(emerge_time()/kEmerge,0.,1.));return .5f+.5f*std::sqrt(t);}
        return 1;
    }
    float camera_fraction()const {
        const double t=travel>0?std::clamp((time-kPull)/travel,0.,1.):1;
        return static_cast<float>(std::sin(t*1.5707963267948966));
    }
};
}
