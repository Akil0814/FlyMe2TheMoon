#pragma once
#include "engine/core/geometry/vector2.h"
#include "engine/input/raw_input_state.h"
#include "game/content/content.h"
#include <algorithm>
#include <numbers>

namespace game::play {
// Match the keyboard's 50-degree steering limit. Down alone never fires the jet.
// Rescale the dead zone so both the stick and trigger retain gradual thrust.
inline elysia::core::Vector2 gamepad_thrust(const elysia::input::RawInputState& state) {
    using C=elysia::input::RawInputControl;
    using A=elysia::input::RawInputAxis;
    const auto axis=[](float value,float dead_zone){
        return std::copysign(std::clamp((std::abs(value)-dead_zone)/(1-dead_zone),0.f,1.f),value);
    };
    float steering=axis(state.axis_value(A::GamepadLeftX),.2f);
    float power=std::max(std::abs(steering),std::max(0.f,-axis(state.axis_value(A::GamepadLeftY),.2f)));
    const bool left=state.is_pressed(C::GamepadDPadLeft),right=state.is_pressed(C::GamepadDPadRight);
    if(left||right){steering=static_cast<float>(right)-static_cast<float>(left);power=1;}
    if(state.is_pressed(C::GamepadDPadUp)||state.is_pressed(C::GamepadSouth))power=1;
    // Match the scene router's neutral threshold so held axes cannot leak
    // through pause/focus/capture suppression at a lower gameplay dead zone.
    power=std::max(power,axis(state.axis_value(A::GamepadRightTrigger),.2f));
    const float angle=steering*50*std::numbers::pi_v<float>/180;
    return {std::sin(angle)*power,-std::cos(angle)*power};
}
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
