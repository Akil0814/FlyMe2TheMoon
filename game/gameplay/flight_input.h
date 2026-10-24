#pragma once
#include "engine/core/geometry/vector2.h"
#include "game/content/content.h"
#include <algorithm>
#include <numbers>

namespace game::play {
// Recovered from the supplied original's RoleBodyComponent.applyPush:pushPoint:.
// Return a dimensionless fraction of maxF in the engine's downward-Y coordinates.
inline elysia::core::Vector2 pointer_thrust(elysia::core::Vector2 role,elysia::core::Vector2 point,const content::Json& config) {
    const auto delta=role-point;
    const float distance=delta.length(),near=config.at("pushDistance1"),far=config.at("pushDistance2");
    if(distance<=near)return {};
    const float angle=std::atan2(delta.x,-delta.y),degrees=std::abs(angle)*180/std::numbers::pi_v<float>;
    const float bound1=config.at("pushAngleBound1"),bound2=config.at("pushAngleBound2");
    const float fade=std::clamp((bound2-degrees)/(bound2-bound1),0.f,1.f)*std::clamp((distance-near)/(far-near),0.f,1.f);
    const float range=config.at("pushAngleRange").get<float>()*std::numbers::pi_v<float>/180;
    const float mapped=std::copysign(std::pow(std::min(std::abs(angle)/range,1.f),config.at("stableFlyRate").get<float>())*range,angle);
    return {std::sin(mapped)*fade,-std::cos(mapped)*fade};
}
inline elysia::core::Vector2 thrust_force(elysia::core::Vector2 thrust,elysia::core::Vector2 velocity,const content::Json& config) {
    auto force=thrust*(config.at("maxF").get<float>()*32);
    const float power=force.x*velocity.x+force.y*velocity.y,maximum=config.at("maxP").get<float>()*32*32;
    if(power>maximum)force*=maximum/power;
    return force;
}
inline elysia::core::Vector2 damp_velocity(elysia::core::Vector2 velocity,bool pushing,float delta,const content::Json& config) {
    velocity.x*=std::clamp(1-delta*config.at("extraXDamping").get<float>(),0.f,1.f);
    velocity.x*=std::clamp(1-delta*config.value(pushing?"extraFlyXDamping":"extraNonFlyXDamping",0.f),0.f,1.f);
    if(pushing)velocity*=std::clamp(1-delta*config.value("extraFlyDamping",0.f),0.f,1.f);
    const float limit=config.at("maxXSpeed").get<float>()*32;
    velocity.x=std::clamp(velocity.x,-limit,limit);return velocity;
}
}
