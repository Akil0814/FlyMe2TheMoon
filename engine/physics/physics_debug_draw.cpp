#include "physics_debug_draw.h"

#include "../tools/debug_draw.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace elysia::physics
{
void submit_physics_debug_snapshot(const PhysicsDebugSnapshot &snapshot,
                                   elysia::tools::DebugDraw &debug_draw)
{
    using elysia::tools::DebugDrawCategory;
    const float alpha = std::clamp(snapshot.interpolation_alpha, 0.0f, 1.0f);
    const auto render_point = [&](elysia::core::Vector2 point, const PhysicsPose &previous,
                                  const PhysicsPose &current) {
        const float delta =
            std::remainder(current.angle - previous.angle, 2 * std::numbers::pi_v<float>);
        const float angle = (alpha - 1) * delta;
        const auto relative = point - current.position;
        const auto origin = previous.position + (current.position - previous.position) * alpha;
        return origin +
               elysia::core::Vector2{std::cos(angle) * relative.x - std::sin(angle) * relative.y,
                                     std::sin(angle) * relative.x + std::cos(angle) * relative.y};
    };
    const auto draw_shape = [&](const WorldColliderShape &shape, DebugDrawCategory category,
                                elysia::core::Color color) {
        if (!debug_draw.is_enabled(category))
            return;
        if (const auto *box = std::get_if<WorldAabb>(&shape))
            debug_draw.draw_rect(category, box->rect, color);
        else if (const auto *polygon = std::get_if<WorldPolygon>(&shape))
        {
            for (int i = 0; i < 4; ++i)
                debug_draw.draw_line(category, polygon->vertices[i], polygon->vertices[(i + 1) % 4],
                                     color);
        }
        else
        {
            const auto &circle = std::get<WorldCircle>(shape);
            debug_draw.draw_circle(category, circle.center, circle.radius, color);
        }
    };
    for (const PhysicsDebugShape &shape : snapshot.shapes)
    {
        auto rendered = shape.current;
        if (auto *polygon = std::get_if<WorldPolygon>(&rendered))
            for (auto &point : polygon->vertices)
                point = render_point(point, shape.previous_pose, shape.current_pose);
        else if (auto *circle = std::get_if<WorldCircle>(&rendered))
            circle->center = render_point(circle->center, shape.previous_pose, shape.current_pose);
        draw_shape(rendered, DebugDrawCategory::PhysicsCollider,
                   shape.sensor  ? elysia::core::Color{210, 120, 255}
                   : shape.awake ? elysia::core::Color{80, 230, 120}
                                 : elysia::core::Color{90, 175, 245});
        draw_shape(shape.previous, DebugDrawCategory::PhysicsPoseHistory, {110, 120, 150, 180});
        draw_shape(shape.current, DebugDrawCategory::PhysicsPoseHistory, {240, 240, 240, 190});
        if (debug_draw.is_enabled(DebugDrawCategory::PhysicsBroadPhase))
            debug_draw.draw_rect(DebugDrawCategory::PhysicsBroadPhase, shape.native_bounds,
                                 {230, 190, 70, 180});
    }
    for (const auto &joint : snapshot.joints)
    {
        auto a = render_point(joint.first_anchor, joint.first_previous, joint.first_current);
        auto b = render_point(joint.second_anchor, joint.second_previous, joint.second_current);
        debug_draw.draw_line(DebugDrawCategory::PhysicsJoint, a, b, {230, 120, 240});
        debug_draw.draw_point(DebugDrawCategory::PhysicsJoint, a, 7, {255, 110, 200});
        debug_draw.draw_point(DebugDrawCategory::PhysicsJoint, b, 4, {100, 220, 255});
    }
    for (const CollisionContact &contact : snapshot.contacts)
    {
        for (std::uint8_t i = 0; i < contact.manifold.contact_point_count; ++i)
        {
            const auto point = contact.manifold.contact_points[i];
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsContact))
                debug_draw.draw_point(DebugDrawCategory::PhysicsContact, point, 4.0f,
                                      {255, 80, 80});
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsContactNormal))
                debug_draw.draw_line(DebugDrawCategory::PhysicsContactNormal, point,
                                     point + contact.manifold.normal * 16.0f, {255, 120, 40});
        }
    }
    for (const PhysicsDebugVelocity &velocity : snapshot.velocities)
    {
        if (debug_draw.is_enabled(DebugDrawCategory::PhysicsVelocity))
            debug_draw.draw_line(DebugDrawCategory::PhysicsVelocity, velocity.origin,
                                 velocity.origin + velocity.velocity * 0.1f, {80, 180, 255});
    }
}
} // namespace elysia::physics
