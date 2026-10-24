#include "detail/physics_world_impl.h"
#include <cmath>
#include <limits>
namespace elysia::physics
{
namespace
{
bool valid_anchor(elysia::core::Vector2 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}
} // namespace
JointHandle PhysicsWorld::create_distance_joint(const DistanceJointDefinition &d)
{
    auto &p = *_impl;
    if (d.first == d.second || !p.get(d.first) || !p.get(d.second) ||
        !valid_anchor(d.local_anchor_first) || !valid_anchor(d.local_anchor_second) ||
        !std::isfinite(d.length) || d.length <= 0 || !std::isfinite(d.frequency_hz) ||
        d.frequency_hz < 0 || !std::isfinite(d.damping_ratio) || d.damping_ratio < 0 ||
        p.next_joint == std::numeric_limits<std::uint64_t>::max())
        return {};
    JointHandle h{p.next_joint++};
    p.joints.emplace(h.value, Impl::Joint{{}, d.first, d.second, false});
    p.enqueue([&p, h, d] {
        auto *a = p.get(d.first);
        auto *b = p.get(d.second);
        if (!a || !b)
        {
            p.joints.erase(h.value);
            return;
        }
        auto def = b2DefaultDistanceJointDef();
        def.bodyIdA = a->native;
        def.bodyIdB = b->native;
        def.localAnchorA = p.to(d.local_anchor_first);
        def.localAnchorB = p.to(d.local_anchor_second);
        def.length = p.units.to_length(d.length);
        def.enableSpring = d.spring;
        def.hertz = d.frequency_hz;
        def.dampingRatio = d.damping_ratio;
        def.collideConnected = d.collide_connected;
        p.joints.at(h.value).native = b2CreateDistanceJoint(p.world, &def);
    });
    return h;
}
JointHandle PhysicsWorld::create_revolute_joint(const RevoluteJointDefinition &d)
{
    auto &p = *_impl;
    if (d.first == d.second || !p.get(d.first) || !p.get(d.second) ||
        !valid_anchor(d.local_anchor_first) || !valid_anchor(d.local_anchor_second) ||
        !std::isfinite(d.reference_angle) || !std::isfinite(d.lower_angle) ||
        !std::isfinite(d.upper_angle) || d.lower_angle > d.upper_angle ||
        !std::isfinite(d.motor_speed) || !std::isfinite(d.max_motor_torque) ||
        d.max_motor_torque < 0 || p.next_joint == std::numeric_limits<std::uint64_t>::max())
        return {};
    JointHandle h{p.next_joint++};
    p.joints.emplace(h.value, Impl::Joint{{}, d.first, d.second, false});
    p.enqueue([&p, h, d] {
        auto *a = p.get(d.first);
        auto *b = p.get(d.second);
        if (!a || !b)
        {
            p.joints.erase(h.value);
            return;
        }
        auto def = b2DefaultRevoluteJointDef();
        def.bodyIdA = a->native;
        def.bodyIdB = b->native;
        def.localAnchorA = p.to(d.local_anchor_first);
        def.localAnchorB = p.to(d.local_anchor_second);
        def.referenceAngle = d.reference_angle;
        def.enableLimit = d.enable_limit;
        def.lowerAngle = d.lower_angle;
        def.upperAngle = d.upper_angle;
        def.enableMotor = d.enable_motor;
        def.motorSpeed = d.motor_speed;
        def.maxMotorTorque = p.units.to_squared(d.max_motor_torque);
        def.collideConnected = d.collide_connected;
        p.joints.at(h.value).native = b2CreateRevoluteJoint(p.world, &def);
    });
    return h;
}
bool PhysicsWorld::destroy_joint(JointHandle h)
{
    auto &p = *_impl;
    auto it = p.joints.find(h.value);
    if (it == p.joints.end() || it->second.removed)
        return false;
    it->second.removed = true;
    p.enqueue([&p, h] {
        auto i = p.joints.find(h.value);
        if (i != p.joints.end())
        {
            if (B2_IS_NON_NULL(i->second.native))
                b2DestroyJoint(i->second.native);
            p.joints.erase(i);
        }
    });
    return true;
}
std::optional<JointState> PhysicsWorld::joint_state(JointHandle h) const noexcept
{
    auto &p = *_impl;
    auto it = p.joints.find(h.value);
    if (it == p.joints.end() || it->second.removed || B2_IS_NULL(it->second.native))
        return {};
    auto id = it->second.native;
    return JointState{
        p.from(b2Body_GetWorldPoint(b2Joint_GetBodyA(id), b2Joint_GetLocalAnchorA(id))),
        p.from(b2Body_GetWorldPoint(b2Joint_GetBodyB(id), b2Joint_GetLocalAnchorB(id))),
        p.from(b2Joint_GetConstraintForce(id)),
        p.units.from_squared(b2Joint_GetConstraintTorque(id))};
}
} // namespace elysia::physics
