#pragma once
#include "engine/physics/collision/collider_shape.h"
#include <cmath>
#include <numbers>

namespace game::play {
// Local reference ARM: Game.loadMapHeaderFromXML (BornPoint branch),
// RoleBodyShaper.createBodyFixturesForRoleBody and its two polygon builders.
inline elysia::core::Vector2 role_spawn(elysia::core::Vector2 born) {return born+elysia::core::Vector2{25,-16};}
inline elysia::physics::ConvexPolygonShape role_upper_shape() {
    elysia::physics::ConvexPolygonShape shape;shape.vertex_count=8;
    shape.local_vertices[0]={-16,0};shape.local_vertices[1]={16,0};
    for(int i=0;i<6;++i){const float a=i*std::numbers::pi_v<float>/5;shape.local_vertices[i+2]={16*std::cos(a),-29-16*std::sin(a)};}
    return shape;
}
inline elysia::physics::ConvexPolygonShape role_sensor_shape() {
    return {{{{15,0},{15,-28},{7.5f,-40.990375f},{-7.5f,-40.990375f},
        {-15,-28},{-15,0},{-7.5f,12.990375f},{7.5f,12.990375f}}},8};
}
}
