#include "detail/physics_world_impl.h"
#include <cmath>
#include <limits>
namespace elysia::physics
{
bool PhysicsWorld::set_target_transform(PhysicsObjectHandle h, PhysicsPose target, double seconds)
{
    auto &p = *_impl;
    auto *object = p.get(h);
    const float dt = static_cast<float>(seconds);
    if (!object || object->definition.type != BodyType::Kinematic || !std::isfinite(seconds) || !std::isfinite(dt) ||
        dt <= 0 || !std::isfinite(1.f / dt) || !std::isfinite(target.position.x) || !std::isfinite(target.position.y) ||
        !std::isfinite(target.angle) || !b2IsValidVec2(p.to(target.position)))
        return false;
    p.enqueue([&p, h, target, dt] {
        auto *object = p.get(h);
        if (!object || B2_IS_NULL(object->native) || !object->owner->is_active() || !b2Body_IsEnabled(object->native))
            return;
        const auto native = object->native;
        const b2Transform pose{p.to(target.position), b2MakeRot(target.angle)};
        const auto center = b2Body_GetWorldCenterOfMass(native);
        const auto desired = b2TransformPoint(pose, b2Body_GetLocalCenterOfMass(native));
        const auto velocity = b2MulSV(1.f / dt, b2Sub(desired, center));
        const float angular = b2RelativeAngle(pose.q, b2Body_GetRotation(native)) / dt;
        if (!b2IsValidVec2(velocity) || !std::isfinite(b2Length(velocity)) || !std::isfinite(angular))
            return;
        // Native target control returns early for sleepy targets. Clear the previous
        // command first so those targets stop instead of retaining stale velocity.
        b2Body_SetLinearVelocity(native, b2Vec2_zero);
        b2Body_SetAngularVelocity(native, 0);
        b2Body_SetTargetTransform(native, pose, dt);
    });
    return true;
}
} // namespace elysia::physics
