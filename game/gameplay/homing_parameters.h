#pragma once
#include "motion_profile.h"
#include "engine/physics/body/body_definition.h"
#include "engine/physics/collision/collider_shape.h"

namespace game::play {
// Original MissleLauncher: anchorInPoint=(16,33.5), spawn=(0,18), angle=0.
inline elysia::core::Vector2 homing_launcher_anchor(elysia::core::Vector2 size) {
    return {16-size.x/2,size.y/2-33.5f};
}
inline elysia::core::Vector2 homing_muzzle_offset(elysia::core::Vector2 size) {
    return homing_launcher_anchor(size)+elysia::core::Vector2{0,-18};
}
inline float homing_power(const content::Json& info) {return static_cast<float>(parameter(info,"Power",1));}
inline float homing_max_speed(const content::Json& info) {return static_cast<float>(parameter(info,"MaxSpeed",-1));}
inline float homing_active_range(const content::Json& info) {return static_cast<float>(parameter(info,"ActiveRange",1000));}
inline elysia::physics::ConvexPolygonShape homing_shape() {
    // Original 9.5 x 24 five-point shape, converted from Y-up to Y-down.
    return {{{{4.75f,-6},{0,-12},{-4.75f,-6},{-4.75f,6},{4.75f,6}}},5};
}
inline elysia::physics::BodyMassData homing_mass_data() {
    // Density=1 kg/m^2, 32 EU/m. Native SetMassData replaces only mass:
    // I_origin=4.1147918701171875; I_COM=I_origin-0.1875*1.6^2.
    return {.1875f,{0,-1.6f},3.6347918701171875f};
}
}
