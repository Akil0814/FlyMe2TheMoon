#pragma once
#include "motion_profile.h"
#include "engine/physics/joint_definition.h"
#include "engine/physics/collision/collider.h"
#include <numbers>
namespace game::play {
inline float platform_width(int kind){return static_cast<float>((kind-104)*32);}
inline elysia::physics::Collider platform_collider(int kind) {
    elysia::physics::Collider c;
    const float width=platform_width(kind);
    c.shape=elysia::physics::BoxShape::from_rect({-width/2,-16,width,32});
    c.density={1};c.material={.friction=1,.restitution=0};
    c.filter={.category=8,.mask=1|2|8};c.tag="platform";
    return c;
}
inline elysia::physics::PrismaticJointDefinition platform_joint(int kind,const content::Json& info,float angle) {
    using namespace elysia::physics;
    const elysia::core::Vector2 travel{static_cast<float>(parameter(info,"X")),static_cast<float>(-parameter(info,"Y"))};
    const float distance=travel.length();
    const float force=static_cast<float>(parameter(info,"MaxForce"));
    return {.local_axis=distance>0?travel/distance:elysia::core::Vector2{1,0},
        .reference_angle=angle*std::numbers::pi_v<float>/180,
        .settings={.enable_limit=true,.lower_translation=0,.upper_translation=distance,
            .enable_motor=true,.max_motor_force=(force>0?force:100*(kind-104))*32}};
}
}
