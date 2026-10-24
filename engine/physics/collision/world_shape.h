#pragma once
#include "../body/body_definition.h"
#include "collider_shape.h"
#include <cmath>
#include <type_traits>
namespace elysia::physics
{
struct WorldCircle
{
    elysia::core::Vector2 center{};
    float radius = 0;
};
struct WorldCapsule
{
    elysia::core::Vector2 first{}, second{};
    float radius = 0;
};
struct WorldPolygon
{
    std::array<elysia::core::Vector2, kMaxPolygonVertices> vertices{};
    std::uint8_t vertex_count = 0;
};
struct WorldSegment {elysia::core::Vector2 first{},second{};bool one_sided=false;};
using WorldColliderShape = std::variant<WorldCircle, WorldCapsule, WorldPolygon, WorldSegment>;
inline WorldColliderShape transform_collider_shape(const ColliderShape &shape, PhysicsPose pose)
{
    const auto rotate = [](elysia::core::Vector2 p, float a) {
        return elysia::core::Vector2{std::cos(a) * p.x - std::sin(a) * p.y, std::sin(a) * p.x + std::cos(a) * p.y};
    };
    const auto world = [&](elysia::core::Vector2 p) { return pose.position + rotate(p, pose.angle); };
    return std::visit(
        [&](const auto &s) -> WorldColliderShape {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, CircleShape>)
                return WorldCircle{world(s.local_center), s.radius};
            else if constexpr (std::is_same_v<T, CapsuleShape>)
                return WorldCapsule{world(s.local_first), world(s.local_second), s.radius};
            else if constexpr (std::is_same_v<T, SegmentShape>)
                return WorldSegment{world(s.local_first),world(s.local_second),false};
            else
            {
                WorldPolygon p;
                if constexpr (std::is_same_v<T, BoxShape>)
                {
                    auto h = s.half_extents;
                    const elysia::core::Vector2 corners[] = {{-h.x, -h.y}, {h.x, -h.y}, {h.x, h.y}, {-h.x, h.y}};
                    p.vertex_count = 4;
                    for (int i = 0; i < 4; ++i)
                        p.vertices[i] = world(s.local_center + rotate(corners[i], s.local_angle));
                }
                else
                {
                    p.vertex_count =
                        static_cast<std::uint8_t>(std::min<std::size_t>(s.vertex_count, kMaxPolygonVertices));
                    for (int i = 0; i < p.vertex_count; ++i)
                        p.vertices[i] = world(s.local_vertices[i]);
                }
                return p;
            }
        },
        shape);
}
} // namespace elysia::physics
