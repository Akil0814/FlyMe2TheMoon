#include "detail/physics_world_impl.h"
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>
namespace elysia::physics
{
namespace
{
b2Vec2 to(elysia::core::Vector2 p, const detail::PhysicsUnits &u)
{
    p = u.to_length(p);
    return {p.x, p.y};
}
bool finite(elysia::core::Vector2 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}
bool nonnegative(float x)
{
    return std::isfinite(x) && x >= 0;
}
bool length(float x, const detail::PhysicsUnits &u)
{
    return std::isfinite(x) && std::isfinite(u.to_length(x));
}
bool force(float x, const detail::PhysicsUnits &u)
{
    return nonnegative(x) && std::isfinite(u.to_length(x));
}
bool torque(float x, const detail::PhysicsUnits &u)
{
    return nonnegative(x) && std::isfinite(u.to_squared(x));
}
bool valid(const DistanceJointSettings &s, const detail::PhysicsUnits &u)
{
    return std::isfinite(s.length) && s.length > 0 && std::isfinite(u.to_length(s.length)) &&
           u.to_length(s.length) > 0 && nonnegative(s.frequency_hz) && nonnegative(s.damping_ratio) &&
           std::isfinite(s.min_length) && s.min_length > 0 && std::isfinite(s.max_length) &&
           s.max_length >= s.min_length && std::isfinite(u.to_length(s.max_length)) && std::isfinite(s.motor_speed) &&
           std::isfinite(u.to_length(s.motor_speed)) && nonnegative(s.max_motor_force) &&
           std::isfinite(u.to_length(s.max_motor_force));
}
bool valid(const RevoluteJointSettings &s, const detail::PhysicsUnits &u)
{
    constexpr float limit = 0.99f * std::numbers::pi_v<float>;
    return nonnegative(s.frequency_hz) && nonnegative(s.damping_ratio) && std::isfinite(s.target_angle) &&
           std::isfinite(s.lower_angle) && std::isfinite(s.upper_angle) && s.lower_angle <= s.upper_angle &&
           s.lower_angle >= -limit && s.upper_angle <= limit && std::isfinite(s.motor_speed) &&
           nonnegative(s.max_motor_torque) && std::isfinite(u.to_squared(s.max_motor_torque));
}

bool valid(const WeldJointSettings &s, const detail::PhysicsUnits &)
{
    return nonnegative(s.linear_frequency_hz) && nonnegative(s.angular_frequency_hz) &&
           nonnegative(s.linear_damping_ratio) && nonnegative(s.angular_damping_ratio);
}
bool valid(const PrismaticJointSettings &s, const detail::PhysicsUnits &u)
{
    return length(s.target_translation, u) && nonnegative(s.frequency_hz) && nonnegative(s.damping_ratio) &&
           length(s.lower_translation, u) && length(s.upper_translation, u) &&
           s.lower_translation <= s.upper_translation && length(s.motor_speed, u) && force(s.max_motor_force, u);
}
bool valid(const WheelJointSettings &s, const detail::PhysicsUnits &u)
{
    return nonnegative(s.frequency_hz) && nonnegative(s.damping_ratio) && length(s.lower_translation, u) &&
           length(s.upper_translation, u) && s.lower_translation <= s.upper_translation &&
           std::isfinite(s.motor_speed) && torque(s.max_motor_torque, u);
}
bool valid(const MouseJointSettings &s, const detail::PhysicsUnits &u)
{
    return finite(s.target) && b2IsValidVec2(to(s.target, u)) && nonnegative(s.frequency_hz) &&
           nonnegative(s.damping_ratio) && force(s.max_force, u);
}
bool valid(const MotorJointSettings &s, const detail::PhysicsUnits &u)
{
    return finite(s.linear_offset) && b2IsValidVec2(to(s.linear_offset, u)) && std::isfinite(s.angular_offset) &&
           force(s.max_force, u) && torque(s.max_torque, u) && nonnegative(s.correction_factor) &&
           s.correction_factor <= 1;
}
void apply(b2JointId id, const DistanceJointSettings &s, const detail::PhysicsUnits &u)
{
    b2DistanceJoint_SetLength(id, u.to_length(s.length));
    b2DistanceJoint_EnableSpring(id, s.enable_spring);
    b2DistanceJoint_SetSpringHertz(id, s.frequency_hz);
    b2DistanceJoint_SetSpringDampingRatio(id, s.damping_ratio);
    b2DistanceJoint_SetLengthRange(id, u.to_length(s.min_length), u.to_length(s.max_length));
    b2DistanceJoint_EnableLimit(id, s.enable_limit);
    b2DistanceJoint_SetMotorSpeed(id, u.to_length(s.motor_speed));
    b2DistanceJoint_SetMaxMotorForce(id, u.to_length(s.max_motor_force));
    b2DistanceJoint_EnableMotor(id, s.enable_motor);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const RevoluteJointSettings &s, const detail::PhysicsUnits &u)
{
    b2RevoluteJoint_EnableSpring(id, s.enable_spring);
    b2RevoluteJoint_SetTargetAngle(id, s.target_angle);
    b2RevoluteJoint_SetSpringHertz(id, s.frequency_hz);
    b2RevoluteJoint_SetSpringDampingRatio(id, s.damping_ratio);
    b2RevoluteJoint_SetLimits(id, s.lower_angle, s.upper_angle);
    b2RevoluteJoint_EnableLimit(id, s.enable_limit);
    b2RevoluteJoint_SetMotorSpeed(id, s.motor_speed);
    b2RevoluteJoint_SetMaxMotorTorque(id, u.to_squared(s.max_motor_torque));
    b2RevoluteJoint_EnableMotor(id, s.enable_motor);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const WeldJointSettings &s, const detail::PhysicsUnits &u)
{
    b2WeldJoint_SetLinearHertz(id, s.linear_frequency_hz);
    b2WeldJoint_SetAngularHertz(id, s.angular_frequency_hz);
    b2WeldJoint_SetLinearDampingRatio(id, s.linear_damping_ratio);
    b2WeldJoint_SetAngularDampingRatio(id, s.angular_damping_ratio);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const PrismaticJointSettings &s, const detail::PhysicsUnits &u)
{
    b2PrismaticJoint_EnableSpring(id, s.enable_spring);
    b2PrismaticJoint_SetTargetTranslation(id, u.to_length(s.target_translation));
    b2PrismaticJoint_SetSpringHertz(id, s.frequency_hz);
    b2PrismaticJoint_SetSpringDampingRatio(id, s.damping_ratio);
    b2PrismaticJoint_SetLimits(id, u.to_length(s.lower_translation), u.to_length(s.upper_translation));
    b2PrismaticJoint_EnableLimit(id, s.enable_limit);
    b2PrismaticJoint_SetMotorSpeed(id, u.to_length(s.motor_speed));
    b2PrismaticJoint_SetMaxMotorForce(id, u.to_length(s.max_motor_force));
    b2PrismaticJoint_EnableMotor(id, s.enable_motor);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const WheelJointSettings &s, const detail::PhysicsUnits &u)
{
    b2WheelJoint_EnableSpring(id, s.enable_spring);
    b2WheelJoint_SetSpringHertz(id, s.frequency_hz);
    b2WheelJoint_SetSpringDampingRatio(id, s.damping_ratio);
    b2WheelJoint_SetLimits(id, u.to_length(s.lower_translation), u.to_length(s.upper_translation));
    b2WheelJoint_EnableLimit(id, s.enable_limit);
    b2WheelJoint_SetMotorSpeed(id, s.motor_speed);
    b2WheelJoint_SetMaxMotorTorque(id, u.to_squared(s.max_motor_torque));
    b2WheelJoint_EnableMotor(id, s.enable_motor);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const MouseJointSettings &s, const detail::PhysicsUnits &u)
{
    b2MouseJoint_SetTarget(id, to(s.target, u));
    b2MouseJoint_SetSpringHertz(id, s.frequency_hz);
    b2MouseJoint_SetSpringDampingRatio(id, s.damping_ratio);
    b2MouseJoint_SetMaxForce(id, u.to_length(s.max_force));
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}
void apply(b2JointId id, const MotorJointSettings &s, const detail::PhysicsUnits &u)
{
    b2MotorJoint_SetLinearOffset(id, b2Body_GetWorldVector(b2Joint_GetBodyA(id), to(s.linear_offset, u)));
    b2MotorJoint_SetAngularOffset(id, s.angular_offset);
    b2MotorJoint_SetMaxForce(id, u.to_length(s.max_force));
    b2MotorJoint_SetMaxTorque(id, u.to_squared(s.max_torque));
    b2MotorJoint_SetCorrectionFactor(id, s.correction_factor);
    b2Joint_SetCollideConnected(id, s.collide_connected);
    b2Joint_WakeBodies(id);
}

template <class Definition>
b2JointId create_native(const Definition &d, b2WorldId world, b2BodyId a, b2BodyId b, const detail::PhysicsUnits &u)
{
    if constexpr (std::is_same_v<Definition, DistanceJointDefinition>)
    {
        auto def = b2DefaultDistanceJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = to(d.local_anchor_first, u);
        def.localAnchorB = to(d.local_anchor_second, u);
        def.length = u.to_length(d.settings.length);
        def.minLength = u.to_length(d.settings.min_length);
        def.maxLength = u.to_length(d.settings.max_length);
        return b2CreateDistanceJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, RevoluteJointDefinition>)
    {
        auto def = b2DefaultRevoluteJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = to(d.local_anchor_first, u);
        def.localAnchorB = to(d.local_anchor_second, u);
        def.referenceAngle = d.reference_angle;
        return b2CreateRevoluteJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, WeldJointDefinition>)
    {
        auto def = b2DefaultWeldJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = to(d.local_anchor_first, u);
        def.localAnchorB = to(d.local_anchor_second, u);
        def.referenceAngle = d.reference_angle;
        return b2CreateWeldJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, PrismaticJointDefinition>)
    {
        auto def = b2DefaultPrismaticJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = to(d.local_anchor_first, u);
        def.localAnchorB = to(d.local_anchor_second, u);
        def.referenceAngle = d.reference_angle;
        const double magnitude = std::hypot(double(d.local_axis.x), double(d.local_axis.y));
        elysia::core::Vector2 axis{float(d.local_axis.x / magnitude), float(d.local_axis.y / magnitude)};
        def.localAxisA = {axis.x, axis.y};
        return b2CreatePrismaticJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, WheelJointDefinition>)
    {
        auto def = b2DefaultWheelJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = to(d.local_anchor_first, u);
        def.localAnchorB = to(d.local_anchor_second, u);
        const double magnitude = std::hypot(double(d.local_axis.x), double(d.local_axis.y));
        elysia::core::Vector2 axis{float(d.local_axis.x / magnitude), float(d.local_axis.y / magnitude)};
        def.localAxisA = {axis.x, axis.y};
        return b2CreateWheelJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, MouseJointDefinition>)
    {
        auto def = b2DefaultMouseJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.target = to(d.settings.target, u);
        return b2CreateMouseJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, MotorJointDefinition>)
    {
        auto def = b2DefaultMotorJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        return b2CreateMotorJoint(world, &def);
    }
    else if constexpr (std::is_same_v<Definition, FilterJointDefinition>)
    {
        auto def = b2DefaultFilterJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        return b2CreateFilterJoint(world, &def);
    }
    return b2_nullJointId;
}
} // namespace

template <class Definition> JointHandle PhysicsWorld::Impl::create_joint(const Definition &d)
{
    const auto *a = get(d.first);
    const auto *b = get(d.second);
    if (d.first == d.second || !a || !b || next_joint == std::numeric_limits<std::uint64_t>::max())
        return {};
    if constexpr (std::is_same_v<Definition, MouseJointDefinition>)
    {
        if (a->definition.type != BodyType::Static || b->definition.type != BodyType::Dynamic)
            return {};
    }
    else if constexpr (!std::is_same_v<Definition, FilterJointDefinition>)
    {
        if (a->definition.type != BodyType::Dynamic && b->definition.type != BodyType::Dynamic)
            return {};
    }
    if constexpr (requires { d.local_anchor_first; })
        if (!finite(d.local_anchor_first) || !finite(d.local_anchor_second) ||
            !b2IsValidVec2(to(d.local_anchor_first)) || !b2IsValidVec2(to(d.local_anchor_second)))
            return {};
    if constexpr (requires { d.reference_angle; })
        if (!std::isfinite(d.reference_angle))
            return {};
    if constexpr (requires { d.local_axis; })
        if (!finite(d.local_axis) || (d.local_axis.x == 0 && d.local_axis.y == 0))
            return {};
    if constexpr (requires { d.settings; })
        if (!valid(d.settings, units))
            return {};
    JointHandle h{next_joint++};
    Joint record{{}, d.first, d.second, {}, false};
    if constexpr (requires { d.settings; })
        record.settings = d.settings;
    else
        record.settings = std::monostate{};
    joints.emplace(h.value, record);
    try
    {
        enqueue([this, h, d] {
            auto i = joints.find(h.value);
            if (i == joints.end() || i->second.removed)
                return;
            const auto *a = get(d.first);
            const auto *b = get(d.second);
            if (!a || !b || B2_IS_NULL(a->native) || B2_IS_NULL(b->native))
            {
                joints.erase(i);
                return;
            }
            i->second.native = create_native(d, world, a->native, b->native, units);
            // Filter and Mouse creation does not retire existing contacts in the bundled
            // native implementation. Force a filter transition at the safe point.
            b2Joint_SetCollideConnected(i->second.native, true);
            if constexpr (requires { d.settings; })
                apply(i->second.native, d.settings, units);
            else
            {
                b2Joint_SetCollideConnected(i->second.native, false);
                b2Joint_WakeBodies(i->second.native);
            }
        });
    }
    catch (...)
    {
        joints.erase(h.value);
        throw;
    }
    return h;
}
template <class Settings> bool PhysicsWorld::Impl::update_joint(JointHandle h, const Settings &s)
{
    auto i = joints.find(h.value);
    if (i == joints.end() || i->second.removed || !get(i->second.first) || !get(i->second.second) ||
        !std::holds_alternative<Settings>(i->second.settings) || !valid(s, units))
        return false;
    enqueue([this, h, s] {
        auto i = joints.find(h.value);
        if (i == joints.end() || i->second.removed || !std::holds_alternative<Settings>(i->second.settings) ||
            B2_IS_NULL(i->second.native) || !get(i->second.first) || !get(i->second.second))
            return;
        apply(i->second.native, s, units);
        i->second.settings = s;
    });
    return true;
}
JointHandle PhysicsWorld::create_distance_joint(const DistanceJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_distance_joint(JointHandle h, const DistanceJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_revolute_joint(const RevoluteJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_revolute_joint(JointHandle h, const RevoluteJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_weld_joint(const WeldJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_weld_joint(JointHandle h, const WeldJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_prismatic_joint(const PrismaticJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_prismatic_joint(JointHandle h, const PrismaticJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_wheel_joint(const WheelJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_wheel_joint(JointHandle h, const WheelJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_mouse_joint(const MouseJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_mouse_joint(JointHandle h, const MouseJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_motor_joint(const MotorJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::update_motor_joint(JointHandle h, const MotorJointSettings &s)
{
    return _impl->update_joint(h, s);
}
JointHandle PhysicsWorld::create_filter_joint(const FilterJointDefinition &d)
{
    return _impl->create_joint(d);
}
bool PhysicsWorld::destroy_joint(JointHandle h)
{
    auto &p = *_impl;
    auto i = p.joints.find(h.value);
    if (i == p.joints.end() || i->second.removed)
        return false;
    // Enqueue before marking removed: allocation failure must leave the joint usable.
    const bool deferred = p.advancing;
    p.enqueue([&p, h] {
        auto i = p.joints.find(h.value);
        if (i == p.joints.end())
            return;
        if (B2_IS_NON_NULL(i->second.native))
        {
            // Restore broad-phase pairs even when both objects remain stationary.
            b2Joint_SetCollideConnected(i->second.native, true);
            b2Joint_WakeBodies(i->second.native);
            b2DestroyJoint(i->second.native);
        }
        p.joints.erase(i);
    });
    if (deferred)
        i->second.removed = true;
    return true;
}

void PhysicsWorld::Impl::refresh_joint_targets()
{
    // This Box2D version consumes motor offsets in world axes. The public target
    // is local to body A, so resolve it again before each fixed simulation step.
    for (const auto &[id, joint] : joints)
        if (!joint.removed && B2_IS_NON_NULL(joint.native))
            if (const auto *settings = std::get_if<MotorJointSettings>(&joint.settings))
                b2MotorJoint_SetLinearOffset(
                    joint.native, b2Body_GetWorldVector(b2Joint_GetBodyA(joint.native), to(settings->linear_offset)));
}
std::optional<JointState> PhysicsWorld::Impl::read_joint_state(JointHandle h) const noexcept
{
    const auto &p = *this;
    auto i = p.joints.find(h.value);
    if (i == p.joints.end() || i->second.removed || B2_IS_NULL(i->second.native) || !p.get(i->second.first) ||
        !p.get(i->second.second))
        return {};
    auto id = i->second.native;
    auto a = b2Joint_GetBodyA(id), b = b2Joint_GetBodyB(id);
    JointState result;
    result.first = i->second.first;
    result.second = i->second.second;
    if (std::holds_alternative<std::monostate>(i->second.settings))
    {
        result.anchor_first = p.from(b2Body_GetPosition(a));
        result.anchor_second = p.from(b2Body_GetPosition(b));
        result.details = FilterJointState{};
        return result;
    }
    const auto pa = b2Body_GetWorldPoint(a, b2Joint_GetLocalAnchorA(id)),
               pb = b2Body_GetWorldPoint(b, b2Joint_GetLocalAnchorB(id));
    result.anchor_first = p.from(pa);
    result.anchor_second = p.from(pb);
    result.reaction_force = p.from(b2Joint_GetConstraintForce(id));
    result.reaction_torque = p.units.from_squared(b2Joint_GetConstraintTorque(id));
    const auto va = b2Body_GetWorldPointVelocity(a, pa), vb = b2Body_GetWorldPointVelocity(b, pb);
    const float angular = b2Body_GetAngularVelocity(b) - b2Body_GetAngularVelocity(a);
    const float relative = b2RelativeAngle(b2Body_GetRotation(b), b2Body_GetRotation(a));
    std::visit(
        [&](const auto &settings) {
            using T = std::decay_t<decltype(settings)>;
            if constexpr (std::is_same_v<T, DistanceJointSettings>)
            {
                auto effective = settings;
                effective.length = p.units.from_length(b2DistanceJoint_GetLength(id));
                effective.min_length = p.units.from_length(b2DistanceJoint_GetMinLength(id));
                effective.max_length = p.units.from_length(b2DistanceJoint_GetMaxLength(id));
                result.details =
                    DistanceJointState{effective, p.units.from_length(b2DistanceJoint_GetCurrentLength(id)),
                                       p.units.from_length(b2Dot(b2Sub(vb, va), b2Normalize(b2Sub(pb, pa)))),
                                       p.units.from_length(b2DistanceJoint_GetMotorForce(id))};
            }
            else if constexpr (std::is_same_v<T, RevoluteJointSettings>)
                result.details = RevoluteJointState{settings, b2RevoluteJoint_GetAngle(id), angular,
                                                    p.units.from_squared(b2RevoluteJoint_GetMotorTorque(id))};
            else if constexpr (std::is_same_v<T, WeldJointSettings>)
                result.details = WeldJointState{
                    settings, p.from(b2Sub(pb, pa)),
                    std::remainder(relative - b2Joint_GetReferenceAngle(id), 2 * std::numbers::pi_v<float>)};
            else if constexpr (std::is_same_v<T, PrismaticJointSettings>)
                result.details = PrismaticJointState{settings, p.units.from_length(b2PrismaticJoint_GetTranslation(id)),
                                                     p.units.from_length(b2PrismaticJoint_GetSpeed(id)),
                                                     p.units.from_length(b2PrismaticJoint_GetMotorForce(id))};
            else if constexpr (std::is_same_v<T, WheelJointSettings>)
            {
                auto axis = b2Body_GetWorldVector(a, b2Joint_GetLocalAxisA(id));
                auto delta = b2Sub(pb, pa);
                auto derivative = b2MulSV(b2Body_GetAngularVelocity(a), b2LeftPerp(axis));
                result.details =
                    WheelJointState{settings, p.units.from_length(b2Dot(delta, axis)),
                                    p.units.from_length(b2Dot(b2Sub(vb, va), axis) + b2Dot(delta, derivative)), angular,
                                    p.units.from_squared(b2WheelJoint_GetMotorTorque(id))};
            }
            else if constexpr (std::is_same_v<T, MouseJointSettings>)
            {
                result.anchor_first = settings.target;
                result.details = MouseJointState{settings, p.from(pb), settings.target - p.from(pb)};
            }
            else if constexpr (std::is_same_v<T, MotorJointSettings>)
            {
                auto local = b2Body_GetLocalPoint(a, b2Body_GetPosition(b));
                result.details =
                    MotorJointState{settings, p.from(local) - settings.linear_offset,
                                    std::remainder(relative - settings.angular_offset, 2 * std::numbers::pi_v<float>)};
            }
        },
        i->second.settings);
    return result;
}
std::optional<JointState> PhysicsWorld::joint_state(JointHandle h) const noexcept
{
    return _impl->read_joint_state(h);
}
} // namespace elysia::physics
