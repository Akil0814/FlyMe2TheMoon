#pragma once
#include "motion_profile.h"
#include "engine/core/geometry/vector2.h"
#include <numbers>

namespace game::play {
inline float angle_difference(float desired,float current) {return std::remainder(desired-current,2*std::numbers::pi_v<float>);}
inline elysia::core::Vector2 rotate_vector(elysia::core::Vector2 p,float radians) {
    return {p.x*std::cos(radians)-p.y*std::sin(radians),p.x*std::sin(radians)+p.y*std::cos(radians)};
}
inline elysia::core::Vector2 wind_force(elysia::core::Vector2 body,elysia::core::Vector2 center,elysia::core::Vector2 size,float radians,const content::Json& info) {
    // Original getIntersectRateWithRole samples two columns and three rows of
    // the role envelope (32 x 61 EU, bottom 16 below its body anchor).
    int inside=0;
    for(float x:{-16.f/3,16.f/3})for(float y:{.75f,-14.5f,-29.75f}) {
        const auto local=rotate_vector(body+elysia::core::Vector2{x,y}-center,-radians);
        if(std::abs(local.x)<=size.x/2&&std::abs(local.y)<=size.y/2)++inside;
    }
    // Fade controls edge artwork, not force attenuation. At zero rotation the
    // original ori_ points left; Strength is a force in the original meter units.
    return rotate_vector({-static_cast<float>(parameter(info,"Strength",20)*32)*inside/6,0},radians);
}
}
