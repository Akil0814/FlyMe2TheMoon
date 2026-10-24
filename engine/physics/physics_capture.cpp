#include "detail/physics_world_impl.h"
#include <type_traits>
#include <utility>
namespace elysia::physics
{
void PhysicsWorld::Impl::capture_debug(PhysicsDebugCapture requested)
{
    static_assert(std::is_nothrow_swappable_v<PhysicsDebugSnapshot>);
    auto &next = debug_spare;
    next.clear();
    if (requested == PhysicsDebugCapture::None)
    {
        std::swap(debug, next);
        return;
    }
    auto add = [&](const Shape &s) {
        if (B2_IS_NULL(s.native))
            return;
        PhysicsDebugShape out;
        out.target = s.target;
        auto transform = b2Body_GetTransform(b2Shape_GetBody(s.native));
        if (!b2Body_IsEnabled(b2Shape_GetBody(s.native)))
            return;
        out.sensor = b2Shape_IsSensor(s.native);
        out.awake = b2Body_IsAwake(b2Shape_GetBody(s.native));
        auto previous_transform = transform;
        if (const auto *object = get(s.owner))
            previous_transform = {to(object->previous.position), b2MakeRot(object->previous.angle)};
        out.previous_pose = {from(previous_transform.p), b2Rot_GetAngle(previous_transform.q)};
        out.current_pose = {from(transform.p), b2Rot_GetAngle(transform.q)};
        const auto capture_shape = [&](b2Transform pose) -> WorldColliderShape {
            switch (b2Shape_GetType(s.native))
            {
            case b2_segmentShape:
            {
                auto segment=b2Shape_GetSegment(s.native);
                return WorldSegment{from(b2TransformPoint(pose,segment.point1)),from(b2TransformPoint(pose,segment.point2)),false};
            }
            case b2_chainSegmentShape:
            {
                auto segment=b2Shape_GetChainSegment(s.native).segment;
                return WorldSegment{from(b2TransformPoint(pose,segment.point1)),from(b2TransformPoint(pose,segment.point2)),true};
            }
            case b2_circleShape: {
                auto circle = b2Shape_GetCircle(s.native);
                return WorldCircle{from(b2TransformPoint(pose, circle.center)), units.from_length(circle.radius)};
            }
            case b2_capsuleShape: {
                auto capsule = b2Shape_GetCapsule(s.native);
                return WorldCapsule{from(b2TransformPoint(pose, capsule.center1)),
                                    from(b2TransformPoint(pose, capsule.center2)), units.from_length(capsule.radius)};
            }
            default: {
                auto polygon = b2Shape_GetPolygon(s.native);
                WorldPolygon result;
                result.vertex_count = static_cast<std::uint8_t>(polygon.count);
                for (int i = 0; i < polygon.count; ++i)
                    result.vertices[i] = from(b2TransformPoint(pose, polygon.vertices[i]));
                return result;
            }
            }
        };
        out.current = capture_shape(transform);
        out.previous = capture_shape(previous_transform);
        auto aabb = b2Shape_GetAABB(s.native);
        out.native_bounds = elysia::core::Rect::from_points(from(aabb.lowerBound), from(aabb.upperBound));
        next.shapes.push_back(out);
    };
    if (captures_physics_debug(requested, PhysicsDebugCapture::Shapes) ||
        captures_physics_debug(requested, PhysicsDebugCapture::BroadPhase))
    {
        for (auto &[id, s] : shapes)
            add(s);
        for (auto &[id, s] : tile_shapes)
            add(s);
    }
    if (captures_physics_debug(requested, PhysicsDebugCapture::Contacts))
        next.contacts.assign(cache.contacts().begin(), cache.contacts().end());
    if (captures_physics_debug(requested, PhysicsDebugCapture::Joints))
        for (const auto &[id, joint] : joints)
        {
            const auto *a = get(joint.first), *b = get(joint.second);
            if (joint.removed || !a || !b || B2_IS_NULL(joint.native))
                continue;
            const auto* mouse=std::get_if<MouseJointSettings>(&joint.settings);
            next.joints.push_back({JointHandle{id},
                                   mouse?mouse->target:from(b2Body_GetWorldPoint(a->native, b2Joint_GetLocalAnchorA(joint.native))),
                                   from(b2Body_GetWorldPoint(b->native, b2Joint_GetLocalAnchorB(joint.native))),
                                   a->previous, a->current, b->previous, b->current,
                                   std::holds_alternative<std::monostate>(joint.settings)});
        }
    if (captures_physics_debug(requested, PhysicsDebugCapture::Velocities))
        for (auto &[id, o] : objects)
            next.velocities.push_back({{id}, o.current.position, from(b2Body_GetLinearVelocity(o.native))});
    std::swap(debug, next);
}
} // namespace elysia::physics
