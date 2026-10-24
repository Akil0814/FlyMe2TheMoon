#pragma once
#include "engine/core/geometry/vector2.h"
#include "game/content/content.h"
#include <algorithm>
#include <numbers>

namespace game::play {
// The original config supplies angle ranges, but not their transfer function.
// Use a central vertical cone, smoothly increasing lateral thrust, and reject
// touches above the outer bound. This interpretation remains an approximation.
inline elysia::core::Vector2 pointer_thrust(elysia::core::Vector2 role,elysia::core::Vector2 point,const content::Json& config) {
    const auto delta=role-point;
    if(std::hypot(delta.x,delta.y)<=4)return {};
    const float angle=std::atan2(std::abs(delta.x),-delta.y)*180/std::numbers::pi_v<float>;
    const float cone=config.at("pushAngleRange").get<float>()*.5f;
    const float bound=config.at("pushAngleBound1");
    if(angle>config.at("pushAngleBound2").get<float>())return {};
    const float mapped=std::clamp((angle-cone)/(bound-cone),0.f,1.f)*std::numbers::pi_v<float>*.5f;
    return {std::copysign(std::sin(mapped),delta.x),-std::cos(mapped)};
}
}
