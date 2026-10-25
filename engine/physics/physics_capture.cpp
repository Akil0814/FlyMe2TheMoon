#include "detail/physics_world_impl.h"
#include <type_traits>
#include <utility>
namespace elysia::physics
{
bool PhysicsWorld::Impl::debug_visible(const Object *o) const noexcept
{
    return o && !o->removed && !o->pending_creation && o->owner && !o->owner->is_destroyed() && o->owner->is_active() &&
           o->definition.enabled && B2_IS_NON_NULL(o->native) && b2Body_IsEnabled(o->native);
}
WorldColliderShape PhysicsWorld::Impl::debug_geometry(b2ShapeId id, b2Transform pose) const
{
    switch (b2Shape_GetType(id))
    {
    case b2_segmentShape:
    case b2_chainSegmentShape: {
        const bool one_sided = b2Shape_GetType(id) == b2_chainSegmentShape;
        auto edge = one_sided ? b2Shape_GetChainSegment(id).segment : b2Shape_GetSegment(id);
        return WorldSegment{from(b2TransformPoint(pose, edge.point1)), from(b2TransformPoint(pose, edge.point2)),
                            one_sided};
    }
    case b2_circleShape: {
        auto circle = b2Shape_GetCircle(id);
        return WorldCircle{from(b2TransformPoint(pose, circle.center)), units.from_length(circle.radius)};
    }
    case b2_capsuleShape: {
        auto capsule = b2Shape_GetCapsule(id);
        return WorldCapsule{from(b2TransformPoint(pose, capsule.center1)),
                            from(b2TransformPoint(pose, capsule.center2)), units.from_length(capsule.radius)};
    }
    default: {
        auto polygon = b2Shape_GetPolygon(id);
        WorldPolygon result;
        result.vertex_count = static_cast<std::uint8_t>(polygon.count);
        for (int n = 0; n < polygon.count; ++n)
            result.vertices[n] = from(b2TransformPoint(pose, polygon.vertices[n]));
        return result;
    }
    }
}
void PhysicsWorld::Impl::capture_debug(PhysicsDebugCapture requested)
{
    static_assert(std::is_nothrow_swappable_v<PhysicsDebugSnapshot>);
    auto &next = debug_spare;
    next.clear();
    next.units_per_meter = config.units_per_meter;
    next.interpolation_alpha = static_cast<float>(interpolation_alpha);
    if (requested == PhysicsDebugCapture::None)
    {
        swap(debug, next);
        return;
    }
    auto add = [&](const Shape &s) {
        if (B2_IS_NULL(s.native))
            return;
        const auto *object = get(s.owner);
        if (s.owner.is_valid() && !debug_visible(object))
            return;
        if (s.chain.is_valid())
        {
            auto c = chains.find(s.chain.value);
            if (c == chains.end() || c->second->removed)
                return;
        }
        auto body = b2Shape_GetBody(s.native);
        if (!b2Body_IsEnabled(body))
            return;
        auto pose = b2Body_GetTransform(body), previous = pose;
        if (object)
            previous = {to(object->previous.position), b2MakeRot(object->previous.angle)};
        PhysicsDebugShape out;
        out.target = s.target;
        out.sensor = b2Shape_IsSensor(s.native);
        out.awake = b2Body_IsAwake(body);
        out.previous_pose = {from(previous.p), b2Rot_GetAngle(previous.q)};
        out.current_pose = {from(pose.p), b2Rot_GetAngle(pose.q)};
        out.previous = debug_geometry(s.native, previous);
        out.current = debug_geometry(s.native, pose);
        auto bounds = b2Shape_GetAABB(s.native);
        out.native_bounds = elysia::core::Rect::from_points(from(bounds.lowerBound), from(bounds.upperBound));
        next.shapes.push_back(out);
    };
    if (captures_physics_debug(requested, PhysicsDebugCapture::Shapes) ||
        captures_physics_debug(requested, PhysicsDebugCapture::BroadPhase))
    {
        for (const auto &[id, object] : objects)
            if (debug_visible(&object))
                if (auto state = read_body_state({id}))
                    next.bodies.push_back({{id}, object.definition.mass_properties, *state});
        for (const auto &[id, s] : shapes)
            add(s);
        for (const auto &[id, s] : tile_shapes)
            add(s);
    }
    if (captures_physics_debug(requested, PhysicsDebugCapture::Contacts))
        next.contacts.assign(cache.contacts().begin(), cache.contacts().end());
    if (captures_physics_debug(requested, PhysicsDebugCapture::Joints))
        for (const auto &[id, joint] : joints)
        {
            const auto *a = get(joint.first), *b = get(joint.second);
            if (joint.removed || !debug_visible(a) || !debug_visible(b) || B2_IS_NULL(joint.native))
                continue;
            auto state = read_joint_state({id});
            if (!state)
                continue;
            PhysicsDebugJoint out;
            out.handle = {id};
            out.state = *state;
            out.first_previous = a->previous;
            out.first_current = a->current;
            out.second_previous = b->previous;
            out.second_current = b->current;
            out.local_anchor_first = from(b2Joint_GetLocalAnchorA(joint.native));
            out.local_anchor_second = from(b2Joint_GetLocalAnchorB(joint.native));
            if (std::holds_alternative<PrismaticJointSettings>(joint.settings) ||
                std::holds_alternative<WheelJointSettings>(joint.settings))
            {
                auto axis = b2Joint_GetLocalAxisA(joint.native);
                out.local_axis_first = {axis.x, axis.y};
            }
            if (std::holds_alternative<PrismaticJointSettings>(joint.settings) ||
                std::holds_alternative<RevoluteJointSettings>(joint.settings) ||
                std::holds_alternative<WeldJointSettings>(joint.settings))
                out.reference_angle = b2Joint_GetReferenceAngle(joint.native);
            next.joints.push_back(out);
        }
    if (captures_physics_debug(requested, PhysicsDebugCapture::Velocities))
        for (const auto &[id, o] : objects)
            if (debug_visible(&o))
                next.velocities.push_back(
                    {{id}, from(b2Body_GetPosition(o.native)), from(b2Body_GetLinearVelocity(o.native))});
    if (captures_physics_debug(requested, PhysicsDebugCapture::Chains))
        for (const auto &[id, chain] : chains)
        {
            const auto *owner = get(chain->owner);
            if (chain->removed || B2_IS_NULL(chain->native) || !debug_visible(owner))
                continue;
            auto &out = next.chains.emplace_back();
            out.handle = {id};
            out.owner = chain->owner;
            out.loop = chain->definition.loop;
            out.local_vertices = chain->definition.local_vertices;
            out.colliders = chain->colliders;
            out.previous = owner->previous;
            out.current = owner->current;
            if (!out.loop)
            {
                out.ghost_before = from(chain->points.front());
                out.ghost_after = from(chain->points.back());
            }
        }
    if (captures_physics_debug(requested, PhysicsDebugCapture::Targets))
        for (const auto &[id, o] : objects)
        {
            if (!debug_visible(&o))
                continue;
            const auto &record = o.pending_debug_target ? o.pending_debug_target : o.completed_debug_target;
            if (!record)
                continue;
            auto &out = next.targets.emplace_back();
            out.owner = {id};
            out.pose_at_command = record->pose_at_command;
            out.requested_pose = record->requested_pose;
            out.effective_pose = record->effective_pose;
            out.fixed_delta_seconds = record->fixed_delta_seconds;
            out.pending = o.pending_debug_target.has_value();
            out.previous = o.previous;
            out.current = o.current;
            const b2Transform target_pose{to(out.effective_pose.position), b2MakeRot(out.effective_pose.angle)};
            for (const auto &[shape_id, shape] : shapes)
                if (shape.owner == out.owner && B2_IS_NON_NULL(shape.native))
                {
                    if (shape.chain.is_valid() && chains.at(shape.chain.value)->removed)
                        continue;
                    out.shapes.push_back(debug_geometry(shape.native, target_pose));
                }
        }
    swap(debug, next);
}
} // namespace elysia::physics
