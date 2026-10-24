#pragma once
#include "motion_profile.h"
#include "engine/core/geometry/vector2.h"
#include <numbers>

namespace game::play {
inline float angle_difference(float desired,float current) {return std::remainder(desired-current,2*std::numbers::pi_v<float>);}
inline elysia::core::Vector2 rotate_vector(elysia::core::Vector2 p,float radians) {
    return {p.x*std::cos(radians)-p.y*std::sin(radians),p.x*std::sin(radians)+p.y*std::cos(radians)};
}
inline elysia::core::Vector2 wind_force(elysia::core::Vector2 point,elysia::core::Vector2 center,elysia::core::Vector2 size,float radians,const content::Json& info) {
    const auto local=rotate_vector(point-center,-radians);
    if(std::abs(local.x)>size.x/2||std::abs(local.y)>size.y/2)return {};
    float fade=1;const std::string ends=info.value("Fade",std::string("00"));
    if(ends.size()>0&&ends[0]=='1')fade*=std::clamp((size.y/2-local.y)/32.f,0.f,1.f);
    if(ends.size()>1&&ends[1]=='1')fade*=std::clamp((size.y/2+local.y)/32.f,0.f,1.f);
    return rotate_vector({0,-static_cast<float>(parameter(info,"Strength",10)*32)*fade},radians);
}
}
