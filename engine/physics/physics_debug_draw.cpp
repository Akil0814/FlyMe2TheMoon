#include "physics_debug_draw.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>
namespace elysia::physics
{
namespace
{
using elysia::core::Color;
using elysia::core::Vector2;
using C = elysia::tools::DebugDrawCategory;
constexpr float pi = std::numbers::pi_v<float>;
constexpr Color yellow{250, 205, 80}, cyan{70, 220, 235, 150}, orange{245, 150, 60}, gray{150, 160, 180};
Vector2 direction(float angle)
{
    return {std::cos(angle), std::sin(angle)};
}
Vector2 rotate(Vector2 p, float angle)
{
    auto axis = direction(angle);
    return {axis.x * p.x - axis.y * p.y, axis.y * p.x + axis.x * p.y};
}
Vector2 point(Vector2 p, PhysicsPose pose)
{
    return pose.position + rotate(p, pose.angle);
}
PhysicsPose interpolated(PhysicsPose previous, PhysicsPose current, float alpha)
{
    return {previous.position + (current.position - previous.position) * alpha,
            previous.angle + std::remainder(current.angle - previous.angle, 2 * pi) * alpha};
}
struct Painter
{
    elysia::tools::DebugDraw &draw;
    float unit;
    void line(C category, Vector2 a, Vector2 b, Color color) const
    {
        draw.draw_line(category, a, b, color);
    }
    void mark(C category, Vector2 p, Color color, float size = 5) const
    {
        draw.draw_point(category, p, size, color);
    }
    void arrow(C category, Vector2 a, Vector2 b, Color color) const
    {
        line(category, a, b, color);
        auto delta = b - a;
        auto axis = delta.normalized();
        const float head = std::min(delta.length() * .3f, .04f * unit);
        if (!std::isfinite(head) || head <= 0)
            return;
        Vector2 side{-axis.y, axis.x};
        line(category, b, b - axis * head + side * (head * .5f), color);
        line(category, b, b - axis * head - side * (head * .5f), color);
    }
    void dashed(C category, Vector2 a, Vector2 b, Color color) const
    {
        for (int n = 0; n < 10; n += 2)
            line(category, a + (b - a) * (n / 10.f), a + (b - a) * ((n + 1) / 10.f), color);
    }
    void arc(C category, Vector2 center, float radius, float start, float end, Color color) const
    {
        if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(end - start))
            return;
        int steps = static_cast<int>(std::clamp(std::ceil(std::abs(end - start) / (pi / 36)), 1.f, 72.f));
        for (int n = 0; n < steps; ++n)
            line(category, center + direction(start + (end - start) * n / steps) * radius,
                 center + direction(start + (end - start) * (n + 1) / steps) * radius, color);
    }
    void spring(C category, Vector2 a, Vector2 b, Color color) const
    {
        auto delta = b - a;
        if (delta.is_zero() || !std::isfinite(delta.length()))
            return;
        auto axis = delta.normalized();
        Vector2 side{-axis.y, axis.x};
        auto previous = a;
        for (int n = 1; n <= 12; ++n)
        {
            auto next = a + delta * (n / 12.f);
            if (n < 12)
                next += side * ((n % 2 ? 1.f : -1.f) * std::min(.04f * unit, delta.length() * .08f));
            line(category, previous, next, color);
            previous = next;
        }
    }
    void frame(C category, PhysicsPose pose, Color color) const
    {
        mark(category, pose.position, color);
        arrow(category, pose.position, pose.position + direction(pose.angle) * (.25f * unit), color);
        line(category, pose.position, pose.position + direction(pose.angle + pi / 2) * (.12f * unit), color);
    }
    void range(C category, Vector2 origin, Vector2 axis, float low, float high, Color color) const
    {
        const auto a = origin + axis * low, b = origin + axis * high;
        Vector2 tick{-axis.y * .035f * unit, axis.x * .035f * unit};
        line(category, a, b, color);
        line(category, a - tick, a + tick, color);
        line(category, b - tick, b + tick, color);
    }
    void shape(const WorldColliderShape &geometry, C category, Color color, bool normals = true) const
    {
        if (!draw.is_enabled(category))
            return;
        if (const auto *edge = std::get_if<WorldSegment>(&geometry))
        {
            line(category, edge->first, edge->second, color);
            if (normals && edge->one_sided)
            {
                auto axis = (edge->second - edge->first).normalized();
                auto center = (edge->first + edge->second) * .5f;
                arrow(category, center, center + Vector2{axis.y, -axis.x} * (.12f * unit), color);
            }
        }
        else if (const auto *polygon = std::get_if<WorldPolygon>(&geometry))
        {
            for (unsigned n = 0; n < polygon->vertex_count; ++n)
                line(category, polygon->vertices[n], polygon->vertices[(n + 1) % polygon->vertex_count], color);
        }
        else if (const auto *capsule = std::get_if<WorldCapsule>(&geometry))
        {
            auto delta = capsule->second - capsule->first;
            if (delta.is_zero())
                draw.draw_circle(category, capsule->first, capsule->radius, color);
            else
            {
                const float angle = std::atan2(delta.y, delta.x);
                arc(category, capsule->first, capsule->radius, angle + pi / 2, angle + 3 * pi / 2, color);
                arc(category, capsule->second, capsule->radius, angle - pi / 2, angle + pi / 2, color);
                auto axis = delta.normalized();
                Vector2 offset{-axis.y * capsule->radius, axis.x * capsule->radius};
                line(category, capsule->first + offset, capsule->second + offset, color);
                line(category, capsule->first - offset, capsule->second - offset, color);
            }
        }
        else
        {
            const auto &circle = std::get<WorldCircle>(geometry);
            draw.draw_circle(category, circle.center, circle.radius, color);
        }
    }
};
WorldColliderShape render_shape(WorldColliderShape shape, PhysicsPose previous, PhysicsPose current, float alpha)
{
    auto pose = interpolated(previous, current, alpha);
    auto render = [&](Vector2 p) { return point(rotate(p - current.position, -current.angle), pose); };
    std::visit(
        [&](auto &geometry) {
            using T = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<T, WorldPolygon>)
                for (unsigned i = 0; i < geometry.vertex_count; ++i)
                    geometry.vertices[i] = render(geometry.vertices[i]);
            else if constexpr (std::is_same_v<T, WorldCircle>)
                geometry.center = render(geometry.center);
            else
            {
                geometry.first = render(geometry.first);
                geometry.second = render(geometry.second);
            }
        },
        shape);
    return shape;
}
} // namespace
PhysicsDebugCapture physics_debug_capture_for(const elysia::tools::DebugDraw &draw) noexcept
{
    auto capture = PhysicsDebugCapture::None;
    if (draw.is_enabled(C::PhysicsCollider) || draw.is_enabled(C::PhysicsPoseHistory))
        capture |= PhysicsDebugCapture::Shapes;
    if (draw.is_enabled(C::PhysicsBroadPhase))
        capture |= PhysicsDebugCapture::BroadPhase;
    if (draw.is_enabled(C::PhysicsContact) || draw.is_enabled(C::PhysicsContactNormal))
        capture |= PhysicsDebugCapture::Contacts;
    if (draw.is_enabled(C::PhysicsVelocity))
        capture |= PhysicsDebugCapture::Velocities;
    if (draw.is_enabled(C::PhysicsJoint) || draw.is_enabled(C::PhysicsJointDetails))
        capture |= PhysicsDebugCapture::Joints;
    if (draw.is_enabled(C::PhysicsChainDetails))
        capture |= PhysicsDebugCapture::Chains;
    if (draw.is_enabled(C::PhysicsTarget))
        capture |= PhysicsDebugCapture::Targets;
    return capture;
}
void submit_physics_debug_snapshot(const PhysicsDebugSnapshot &snapshot, elysia::tools::DebugDraw &debug_draw)
{
    const float alpha = std::clamp(snapshot.interpolation_alpha, 0.f, 1.f);
    Painter paint{debug_draw, std::isfinite(snapshot.units_per_meter) && snapshot.units_per_meter > 0
                                  ? snapshot.units_per_meter
                                  : 100};
    for (const auto &shape : snapshot.shapes)
    {
        paint.shape(render_shape(shape.current, shape.previous_pose, shape.current_pose, alpha), C::PhysicsCollider,
                    shape.sensor  ? Color{210, 120, 255}
                    : shape.awake ? Color{80, 230, 120}
                                  : Color{90, 175, 245});
        paint.shape(shape.previous, C::PhysicsPoseHistory, {110, 120, 150, 180});
        paint.shape(shape.current, C::PhysicsPoseHistory, {240, 240, 240, 190});
        debug_draw.draw_rect(C::PhysicsBroadPhase, shape.native_bounds, {230, 190, 70, 180});
    }
    for (const auto &joint : snapshot.joints)
    {
        auto first = interpolated(joint.first_previous, joint.first_current, alpha),
             second = interpolated(joint.second_previous, joint.second_current, alpha);
        auto a = point(joint.local_anchor_first, first), b = point(joint.local_anchor_second, second);
        const bool filter = std::holds_alternative<FilterJointState>(joint.state.details);
        if (filter)
        {
            a = first.position;
            b = second.position;
        }
        if (const auto *mouse = std::get_if<MouseJointState>(&joint.state.details))
            a = mouse->settings.target;
        if (filter)
            paint.dashed(C::PhysicsJoint, a, b, gray);
        else
            paint.line(C::PhysicsJoint, a, b, {230, 120, 240});
        paint.mark(C::PhysicsJoint, a, {255, 110, 200}, 7);
        paint.mark(C::PhysicsJoint, b, {100, 220, 255}, 4);
        if (!debug_draw.is_enabled(C::PhysicsJointDetails))
            continue;
        auto axis = rotate(joint.local_axis_first, first.angle);
        const float glyph = .25f * paint.unit, radius = .2f * paint.unit,
                    reference = first.angle + joint.reference_angle;
        std::visit(
            [&](const auto &state) {
                using T = std::decay_t<decltype(state)>;
                if constexpr (std::is_same_v<T, DistanceJointState>)
                {
                    auto along = (b - a).normalized();
                    if (along.is_zero())
                        along = direction(first.angle);
                    paint.mark(C::PhysicsJointDetails, a + along * state.settings.length, yellow, 6);
                    if (state.settings.enable_spring)
                    {
                        paint.spring(C::PhysicsJointDetails, a, b, cyan);
                        if (state.settings.enable_limit)
                            paint.range(C::PhysicsJointDetails, a, along, state.settings.min_length,
                                        state.settings.max_length, yellow);
                    }
                }
                else if constexpr (std::is_same_v<T, RevoluteJointState>)
                {
                    paint.arrow(C::PhysicsJointDetails, a, a + direction(reference) * glyph, gray);
                    paint.arrow(C::PhysicsJointDetails, b, b + direction(second.angle) * glyph, cyan);
                    if (state.settings.enable_limit)
                    {
                        paint.arc(C::PhysicsJointDetails, a, radius, reference + state.settings.lower_angle,
                                  reference + state.settings.upper_angle, yellow);
                        paint.mark(C::PhysicsJointDetails,
                                   a + direction(reference + state.settings.lower_angle) * radius, yellow);
                        paint.mark(C::PhysicsJointDetails,
                                   a + direction(reference + state.settings.upper_angle) * radius, yellow);
                    }
                    if (state.settings.enable_spring)
                        paint.arrow(C::PhysicsJointDetails, a,
                                    a + direction(reference + state.settings.target_angle) * glyph, yellow);
                }
                else if constexpr (std::is_same_v<T, WeldJointState>)
                {
                    paint.frame(C::PhysicsJointDetails, {a, reference}, gray);
                    paint.frame(C::PhysicsJointDetails, {b, second.angle}, cyan);
                    paint.arrow(C::PhysicsJointDetails, a, b, orange);
                    paint.arc(C::PhysicsJointDetails, a, radius, reference,
                              reference + std::remainder(second.angle - reference, 2 * pi), orange);
                }
                else if constexpr (std::is_same_v<T, PrismaticJointState> || std::is_same_v<T, WheelJointState>)
                {
                    paint.arrow(C::PhysicsJointDetails, a - axis * glyph, a + axis * glyph, gray);
                    paint.mark(C::PhysicsJointDetails, a, cyan);
                    if (state.settings.enable_limit)
                        paint.range(C::PhysicsJointDetails, a, axis, state.settings.lower_translation,
                                    state.settings.upper_translation, yellow);
                    if (state.settings.enable_spring)
                        paint.spring(C::PhysicsJointDetails, a, b, cyan);
                    if constexpr (std::is_same_v<T, PrismaticJointState>)
                    {
                        if (state.settings.enable_spring)
                            paint.mark(C::PhysicsJointDetails, a + axis * state.settings.target_translation, yellow, 7);
                    }
                    else
                    {
                        float speed = state.settings.enable_motor ? state.settings.motor_speed : state.angular_velocity;
                        if (speed != 0)
                        {
                            float sign = speed > 0 ? 1.f : -1.f;
                            float end = second.angle + sign * pi * .75f;
                            paint.arc(C::PhysicsJointDetails, b, radius, second.angle, end, cyan);
                            auto tip = b + direction(end) * radius;
                            paint.arrow(C::PhysicsJointDetails,
                                        tip - direction(end + sign * pi / 2) * (.08f * paint.unit), tip, cyan);
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, MouseJointState>)
                {
                    paint.mark(C::PhysicsJointDetails, state.settings.target, cyan, 9);
                    paint.arrow(C::PhysicsJointDetails, b, state.settings.target, orange);
                }
                else if constexpr (std::is_same_v<T, MotorJointState>)
                {
                    auto target = point(state.settings.linear_offset, first);
                    paint.frame(C::PhysicsJointDetails, {target, first.angle + state.settings.angular_offset}, cyan);
                    paint.arrow(C::PhysicsJointDetails, second.position, target, orange);
                    paint.arc(C::PhysicsJointDetails, target, radius, first.angle + state.settings.angular_offset,
                              first.angle + state.settings.angular_offset +
                                  std::remainder(second.angle - first.angle - state.settings.angular_offset, 2 * pi),
                              orange);
                }
                // A Filter has no mechanical axis, spring or limit.
            },
            joint.state.details);
    }
    if (debug_draw.is_enabled(C::PhysicsChainDetails))
        for (const auto &chain : snapshot.chains)
        {
            auto pose = interpolated(chain.previous, chain.current, alpha);
            const auto count = chain.local_vertices.size();
            for (std::size_t n = 0; n < count; ++n)
            {
                auto a = point(chain.local_vertices[n], pose);
                paint.mark(C::PhysicsChainDetails, a, orange, 6);
                if (n + 1 < count || chain.loop)
                    paint.line(C::PhysicsChainDetails, a, point(chain.local_vertices[(n + 1) % count], pose), gray);
            }
            if (count && !chain.loop)
            {
                if (chain.ghost_before)
                {
                    auto ghost = point(*chain.ghost_before, pose);
                    paint.dashed(C::PhysicsChainDetails, ghost, point(chain.local_vertices.front(), pose), orange);
                    paint.mark(C::PhysicsChainDetails, ghost, orange);
                }
                if (chain.ghost_after)
                {
                    auto ghost = point(*chain.ghost_after, pose);
                    paint.dashed(C::PhysicsChainDetails, point(chain.local_vertices.back(), pose), ghost, orange);
                    paint.mark(C::PhysicsChainDetails, ghost, orange);
                }
            }
        }
    if (debug_draw.is_enabled(C::PhysicsTarget))
        for (const auto &target : snapshot.targets)
        {
            auto pose = interpolated(target.previous, target.current, alpha);
            for (const auto &shape : target.shapes)
                paint.shape(shape, C::PhysicsTarget, cyan, false);
            paint.frame(C::PhysicsTarget, target.effective_pose, cyan);
            paint.dashed(C::PhysicsTarget, pose.position, target.effective_pose.position, cyan);
        }
    for (const auto &contact : snapshot.contacts)
        for (unsigned n = 0; n < contact.manifold.contact_point_count; ++n)
        {
            auto p = contact.manifold.contact_points[n];
            paint.mark(C::PhysicsContact, p, {255, 80, 80}, 4);
            paint.line(C::PhysicsContactNormal, p, p + contact.manifold.normal * 16, {255, 120, 40});
        }
    for (const auto &velocity : snapshot.velocities)
        paint.line(C::PhysicsVelocity, velocity.origin, velocity.origin + velocity.velocity * .1f, {80, 180, 255});
}
} // namespace elysia::physics
