#include "contracts/physics_step_participant.h"
#include "detail/physics_world_impl.h"
#include "../tools/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace elysia::physics
{
using detail::ColliderValidationFailure;
using detail::collider_validation_failure;

namespace
{
bool finite(elysia::core::Vector2 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}
bool nonnegative(float v)
{
    return std::isfinite(v) && v >= 0;
}

} // namespace

std::optional<ColliderValidationFailure> detail::collider_validation_failure(
    const Collider& collider)
{
    if (!nonnegative(collider.material.friction))
        return ColliderValidationFailure::Friction;
    if (!nonnegative(collider.material.restitution))
        return ColliderValidationFailure::Restitution;
    if (!nonnegative(collider.density.kilograms_per_square_meter))
        return ColliderValidationFailure::Density;
    if (collider.one_way && !nonnegative(collider.one_way->tolerance))
        return ColliderValidationFailure::OneWayTolerance;

    return std::visit(
        [](const auto& shape) -> std::optional<ColliderValidationFailure>
        {
            using Shape = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<Shape, AabbShape>)
            {
                if (!finite(shape.local_rect.position()))
                    return ColliderValidationFailure::AabbPosition;
                if (!finite(shape.local_rect.size())
                    || shape.local_rect.width() <= 0
                    || shape.local_rect.height() <= 0)
                {
                    return ColliderValidationFailure::AabbSize;
                }
            }
            else
            {
                if (!finite(shape.local_center))
                    return ColliderValidationFailure::CircleCenter;
                if (!std::isfinite(shape.radius) || shape.radius <= 0)
                    return ColliderValidationFailure::CircleRadius;
            }
            return std::nullopt;
        },
        collider.shape);
}

namespace
{
void append_collider_validation_detail(
    std::ostream& stream,
    ColliderValidationFailure failure,
    const Collider& collider)
{
    switch (failure)
    {
    case ColliderValidationFailure::Friction:
        stream << "friction must be finite and non-negative; friction="
               << collider.material.friction;
        return;
    case ColliderValidationFailure::Restitution:
        stream << "restitution must be finite and non-negative; restitution="
               << collider.material.restitution;
        return;
    case ColliderValidationFailure::Density:
        stream << "density must be finite and non-negative; density="
               << collider.density.kilograms_per_square_meter;
        return;
    case ColliderValidationFailure::OneWayTolerance:
        stream << "one-way tolerance must be finite and non-negative; tolerance="
               << collider.one_way->tolerance;
        return;
    case ColliderValidationFailure::AabbPosition:
    case ColliderValidationFailure::AabbSize:
    {
        const auto& shape = std::get<AabbShape>(collider.shape);
        stream << (failure == ColliderValidationFailure::AabbPosition
                       ? "AABB position must be finite"
                       : "AABB size must be finite and positive")
               << "; position=(" << shape.local_rect.position().x << ','
               << shape.local_rect.position().y << ") size=("
               << shape.local_rect.width() << ',' << shape.local_rect.height() << ')';
        return;
    }
    case ColliderValidationFailure::CircleCenter:
    case ColliderValidationFailure::CircleRadius:
    {
        const auto& shape = std::get<CircleShape>(collider.shape);
        stream << (failure == ColliderValidationFailure::CircleCenter
                       ? "circle center must be finite"
                       : "circle radius must be finite and positive")
               << "; center=(" << shape.local_center.x << ',' << shape.local_center.y
               << ") radius=" << shape.radius;
        return;
    }
    }
}

void log_invalid_collider_registration(
    const elysia::core::GameObject& owner,
    std::size_t collider_index,
    ColliderValidationFailure failure,
    const Collider& collider)
{
    ELYSIA_LOG_WARN("physics", "Rejected collider registration: owner_type="
        << typeid(owner).name() << ", collider_index=" << collider_index << ", "
        << [&]() -> std::string {
            std::ostringstream detail;
            append_collider_validation_detail(detail, failure, collider);
            return detail.str();
        }());
}

void log_invalid_collider_update(
    ColliderId id,
    ColliderValidationFailure failure,
    const Collider& collider)
{
    ELYSIA_LOG_WARN("physics", "Rejected collider update: collider_id=" << id << ", "
        << [&]() -> std::string {
            std::ostringstream detail;
            append_collider_validation_detail(detail, failure, collider);
            return detail.str();
        }());
}
b2BodyType type(BodyType t)
{
    return t == BodyType::Dynamic     ? b2_dynamicBody
           : t == BodyType::Kinematic ? b2_kinematicBody
                                      : b2_staticBody;
}
} // namespace
PhysicsWorld::Impl::Impl(PhysicsWorldConfig c) : config(c), units(c.units_per_meter)
{
    if (!c.sub_steps || c.sub_steps > 64 || !finite(c.gravity) ||
        !nonnegative(c.restitution_velocity_threshold) ||
        !nonnegative(c.contact_normal_threshold) || c.contact_normal_threshold > 1)
        throw std::invalid_argument("Invalid physics configuration");
    auto d = b2DefaultWorldDef();
    d.gravity = to(c.gravity);
    d.enableSleep = c.enable_sleep;
    d.restitutionThreshold = units.to_length(c.restitution_velocity_threshold);
    world = b2CreateWorld(&d);
    b2World_SetPreSolveCallback(world, pre_solve, this);
}
PhysicsWorld::Impl::~Impl()
{
    b2DestroyWorld(world);
}
void PhysicsWorld::Impl::create_shape(Shape &s, b2BodyId body)
{
    auto &c = s.definition;
    if (!c.enabled || c.response == CollisionResponse::Ignore)
        return;
    auto d = b2DefaultShapeDef();
    d.isSensor = c.response == CollisionResponse::Overlap;
    d.enableSensorEvents = true;
    d.enableContactEvents = true;
    d.enablePreSolveEvents = true;
    d.filter.categoryBits = c.filter.category;
    d.filter.maskBits = c.filter.mask;
    d.filter.groupIndex = c.filter.group;
    d.material.friction = c.material.friction;
    d.material.restitution = c.material.restitution;
    d.density = d.isSensor && !c.sensor_contributes_mass ? 0 : c.density.kilograms_per_square_meter;
    std::visit(
        [&](auto &shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, AabbShape>)
            {
                auto r = shape.local_rect;
                auto poly =
                    b2MakeOffsetBox(units.to_length(r.width() / 2), units.to_length(r.height() / 2),
                                    to(r.center()), b2Rot_identity);
                s.native = b2CreatePolygonShape(body, &d, &poly);
            }
            else
            {
                b2Circle circle{to(shape.local_center), units.to_length(shape.radius)};
                s.native = b2CreateCircleShape(body, &d, &circle);
            }
        },
        c.shape);
    mapping.emplace(b2StoreShapeId(s.native), Mapping{s.target, 0});
}
void PhysicsWorld::Impl::retire(Shape &s)
{
    cache.invalidate_target(s.target);
    if (B2_IS_NON_NULL(s.native))
    {
        auto it = mapping.find(b2StoreShapeId(s.native));
        if (it != mapping.end())
            it->second.retire_after = epoch + 1;
        b2DestroyShape(s.native, false);
        s.native = b2_nullShapeId;
    }
}
bool PhysicsWorld::Impl::bullet(const Object &o) const
{
    return o.definition.bullet || std::ranges::any_of(o.shapes, [&](ColliderId id) {
        return shapes.at(id).definition.detection_mode == CollisionDetectionMode::Continuous;
    });
}
void PhysicsWorld::Impl::mass(Object &o)
{
    if (o.definition.type != BodyType::Dynamic)
        return;
    b2Body_ApplyMassFromShapes(o.native);
    if (o.definition.mass_policy == MassPolicy::ExplicitMass)
    {
        auto m = b2Body_GetMassData(o.native);
        if (m.mass > 0)
            m.rotationalInertia *= o.definition.mass / m.mass;
        m.mass = o.definition.mass;
        b2Body_SetMassData(o.native, m);
    }
}
void PhysicsWorld::Impl::destroy_object(std::uint64_t h)
{
    auto it = objects.find(h);
    if (it == objects.end())
        return;
    if (it->second.owner)
        it->second.owner->set_position(it->second.current.position);
    for (auto id : it->second.shapes)
    {
        auto s = shapes.find(id);
        if (s != shapes.end())
        {
            retire(s->second);
            shapes.erase(s);
        }
    }
    std::erase_if(joints, [&](auto &pair) {
        return pair.second.first.value == h || pair.second.second.value == h;
    });
    if (B2_IS_NON_NULL(it->second.native))
        b2DestroyBody(it->second.native);
    objects.erase(it);
}
void PhysicsWorld::Impl::clear_tiles()
{
    for (auto &[coord, s] : tile_shapes)
        retire(s);
    tile_shapes.clear();
    if (B2_IS_NON_NULL(tile_body))
        b2DestroyBody(tile_body);
    tile_body = b2_nullBodyId;
    tiles = nullptr;
}
void PhysicsWorld::Impl::clear()
{
    for (auto &[id, o] : objects)
        if (o.owner)
            o.owner->set_position(o.current.position);
    commands.clear();
    joints.clear();
    objects.clear();
    shapes.clear();
    tile_shapes.clear();
    mapping.clear();
    snapshot.clear();
    ignored.clear();
    previous_blocking_contacts.clear();
    cache.clear();
    listeners.clear();
    b2DestroyWorld(world);
    auto d = b2DefaultWorldDef();
    d.gravity = to(config.gravity);
    d.enableSleep = config.enable_sleep;
    d.restitutionThreshold = units.to_length(config.restitution_velocity_threshold);
    world = b2CreateWorld(&d);
    b2World_SetPreSolveCallback(world, pre_solve, this);
    tile_body = b2_nullBodyId;
    tiles = nullptr;
    interpolation_alpha = 0.0;
    pending_reset = false;
    stats = {};
    debug.clear();
    debug_spare.clear();
}
void PhysicsWorld::Impl::flush()
{
    if (pending_reset)
    {
        clear();
        return;
    }
    auto pending = std::move(commands);
    commands.clear();
    try
    {
        for (auto &f : pending)
            f();
    }
    catch (...)
    {
        commands.clear();
        rollback_aborted_registrations();
        throw;
    }
}
void PhysicsWorld::Impl::rollback_registration(std::uint64_t h) noexcept
{
    auto object = objects.find(h);
    if (object == objects.end())
        return;
    std::erase_if(joints, [h](const auto& entry) {
        return entry.second.first.value == h || entry.second.second.value == h;
    });
    for (auto id : object->second.shapes)
    {
        auto shape = shapes.find(id);
        if (shape == shapes.end())
            continue;
        if (B2_IS_NON_NULL(shape->second.native))
            mapping.erase(b2StoreShapeId(shape->second.native));
        shapes.erase(shape);
    }
    if (B2_IS_NON_NULL(object->second.native))
        b2DestroyBody(object->second.native);
    objects.erase(object);
}
void PhysicsWorld::Impl::rollback_aborted_registrations() noexcept
{
    for (auto it = objects.begin(); it != objects.end();)
    {
        if (!it->second.pending_creation && !it->second.removed)
        {
            ++it;
            continue;
        }
        const auto id = it++->first;
        rollback_registration(id);
    }
}
PhysicsWorld::PhysicsWorld(PhysicsWorldConfig c) : _impl(std::make_unique<Impl>(c))
{
}
PhysicsWorld::~PhysicsWorld() = default;
const PhysicsWorldConfig &PhysicsWorld::config() const noexcept
{
    return _impl->config;
}
const PhysicsStepStats &PhysicsWorld::last_step_stats() const noexcept
{
    return _impl->stats;
}
void PhysicsWorld::reset() noexcept
{
    if (_impl->advancing)
        _impl->pending_reset = true;
    else
        _impl->clear();
}
PhysicsObjectHandle PhysicsWorld::register_object(elysia::core::GameObject &owner,
                                                  const BodyDefinition &d,
                                                  std::span<const Collider> cs)
{
    auto &p = *_impl;
    if (owner.is_destroyed() || !finite(owner.position()) || !finite(d.velocity) ||
        !std::isfinite(d.angle) || !std::isfinite(d.angular_velocity) ||
        !std::isfinite(d.gravity_scale) || !nonnegative(d.linear_damping) ||
        !nonnegative(d.angular_damping) || !std::isfinite(d.mass) || d.mass <= 0)
        return {};
    for (std::size_t index = 0; index < cs.size(); ++index)
    {
        if (const auto failure = collider_validation_failure(cs[index]))
        {
            log_invalid_collider_registration(owner, index, *failure, cs[index]);
            return {};
        }
    }
    for (auto &[id, o] : p.objects)
        if (o.owner == &owner)
            return o.removed ? PhysicsObjectHandle{} : PhysicsObjectHandle{id};
    if (p.next_object == std::numeric_limits<std::uint64_t>::max() ||
        cs.size() >= std::numeric_limits<std::uint64_t>::max() - p.next_shape)
        return {};
    PhysicsObjectHandle h{p.next_object++};
    Impl::Object o;
    o.owner = &owner;
    o.definition = d;
    o.previous = o.current = {owner.position(), d.angle};
    o.pending_creation = true;
    o.shapes.reserve(cs.size());
    try
    {
        for (auto c : cs)
        {
            auto id = p.next_shape++;
            p.shapes.emplace(id, Impl::Shape{{}, h, CollisionTarget::from_collider(id), c});
            try { o.shapes.push_back(id); }
            catch (...) { p.shapes.erase(id); throw; }
        }
        p.objects.emplace(h.value, std::move(o));
    }
    catch (...)
    {
        for (auto id : o.shapes)
            p.shapes.erase(id);
        throw;
    }
    auto create_native = [&p, h] {
        auto it = p.objects.find(h.value);
        if (it == p.objects.end() || it->second.removed || !it->second.owner)
            return;
        auto &o = it->second;
        auto d = b2DefaultBodyDef();
        d.type = type(o.definition.type);
        d.position = p.to(o.current.position);
        d.rotation = b2MakeRot(o.current.angle);
        d.linearVelocity = p.to(o.definition.velocity);
        d.angularVelocity = o.definition.angular_velocity;
        d.gravityScale = o.definition.gravity_scale;
        d.linearDamping = o.definition.linear_damping;
        d.angularDamping = o.definition.angular_damping;
        d.fixedRotation = o.definition.fixed_rotation;
        d.enableSleep = o.definition.enable_sleep;
        d.isEnabled = o.definition.enabled && o.owner->is_active();
        d.isBullet = p.bullet(o);
        o.native = b2CreateBody(p.world, &d);
        for (auto id : o.shapes)
            p.create_shape(p.shapes.at(id), o.native);
        p.mass(o);
        o.pending_creation = false;
    };
    try { p.enqueue(create_native); }
    catch (...)
    {
        p.rollback_registration(h.value);
        throw;
    }
    return h;
}
bool PhysicsWorld::unregister_object(PhysicsObjectHandle h)
{
    auto &p = *_impl;
    auto *o = p.get(h);
    if (!o)
        return false;
    if (o->pending_creation)
    {
        p.rollback_registration(h.value);
        return true;
    }
    if (p.advancing)
        p.enqueue([&p, h] { p.destroy_object(h.value); });
    o->removed = true;
    for (auto &[id, j] : p.joints)
        if (j.first == h || j.second == h)
            j.removed = true;
    if (!p.advancing)
        p.destroy_object(h.value);
    return true;
}
bool PhysicsWorld::contains_object(PhysicsObjectHandle h) const noexcept
{
    return _impl->get(h) != nullptr;
}
bool PhysicsWorld::contains_object(const elysia::core::GameObject &o) const noexcept
{
    return object_handle(o).has_value();
}
std::optional<PhysicsObjectHandle> PhysicsWorld::object_handle(
    const elysia::core::GameObject &o) const noexcept
{
    for (auto &[id, r] : _impl->objects)
        if (r.owner == &o && !r.removed)
            return PhysicsObjectHandle{id};
    return {};
}
bool PhysicsWorld::contains_collider(ColliderId id) const noexcept
{
    auto it = _impl->shapes.find(id);
    return it != _impl->shapes.end() && contains_object(it->second.owner);
}
std::size_t PhysicsWorld::registered_object_count() const noexcept
{
    std::size_t n = 0;
    for (auto &[id, o] : _impl->objects)
        n += !o.removed;
    return n;
}
std::size_t PhysicsWorld::registered_collider_count() const noexcept
{
    std::size_t n = 0;
    for (auto &[id, s] : _impl->shapes)
        n += contains_object(s.owner);
    return n;
}
ColliderId PhysicsWorld::collider_id(PhysicsObjectHandle h, std::size_t index) const noexcept
{
    auto *o = _impl->get(h);
    return o && index < o->shapes.size() ? o->shapes[index] : InvalidColliderId;
}
std::optional<BodyState> PhysicsWorld::body_state(PhysicsObjectHandle h) const noexcept
{
    auto &p = *_impl;
    auto *o = p.get(h);
    if (!o || B2_IS_NULL(o->native))
        return {};
    auto id = o->native;
    return BodyState{p.from(b2Body_GetPosition(id)),
                     p.from(b2Body_GetLinearVelocity(id)),
                     b2Rot_GetAngle(b2Body_GetRotation(id)),
                     b2Body_GetAngularVelocity(id),
                     b2Body_GetMass(id),
                     p.units.from_squared(b2Body_GetRotationalInertia(id)),
                     b2Body_IsAwake(id),
                     b2Body_IsEnabled(id)};
}
std::optional<PhysicsPose> PhysicsWorld::render_pose(PhysicsObjectHandle h) const noexcept
{
    auto *o = _impl->get(h);
    if (!o)
        return {};
    const float a = static_cast<float>(_impl->interpolation_alpha);
    float delta = std::remainder(o->current.angle - o->previous.angle, 2 * 3.14159265358979323846f);
    return PhysicsPose{o->previous.position + (o->current.position - o->previous.position) * a,
                       o->previous.angle + delta * a};
}
bool PhysicsWorld::set_velocity(PhysicsObjectHandle h, elysia::core::Vector2 v)
{
    auto &p = *_impl;
    if (!p.get(h) || !finite(v))
        return false;
    p.enqueue([&p, h, v] {
        auto *o = p.raw(h);
        if (o)
            b2Body_SetLinearVelocity(o->native, p.to(v));
    });
    return true;
}
bool PhysicsWorld::set_velocity_x(PhysicsObjectHandle h, float v)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(v))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
        {
            auto velocity = b2Body_GetLinearVelocity(o->native);
            velocity.x = p.units.to_length(v);
            b2Body_SetLinearVelocity(o->native, velocity);
        }
    });
    return true;
}
bool PhysicsWorld::set_velocity_y(PhysicsObjectHandle h, float v)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(v))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
        {
            auto velocity = b2Body_GetLinearVelocity(o->native);
            velocity.y = p.units.to_length(v);
            b2Body_SetLinearVelocity(o->native, velocity);
        }
    });
    return true;
}
bool PhysicsWorld::set_angular_velocity(PhysicsObjectHandle h, float v)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(v))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
            b2Body_SetAngularVelocity(o->native, v);
    });
    return true;
}
bool PhysicsWorld::set_gravity_scale(PhysicsObjectHandle h, float v)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(v))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
        {
            o->definition.gravity_scale = v;
            b2Body_SetGravityScale(o->native, v);
        }
    });
    return true;
}
bool PhysicsWorld::set_awake(PhysicsObjectHandle h, bool v)
{
    auto &p = *_impl;
    if (!p.get(h))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
            b2Body_SetAwake(o->native, v);
    });
    return true;
}
bool PhysicsWorld::set_body_enabled(PhysicsObjectHandle h, bool v)
{
    auto &p = *_impl;
    if (!p.get(h))
        return false;
    p.enqueue([&p, h, v] {
        if (auto *o = p.raw(h))
        {
            o->definition.enabled = v;
            if (v && o->owner->is_active())
                b2Body_Enable(o->native);
            else
                b2Body_Disable(o->native);
        }
    });
    return true;
}
bool PhysicsWorld::apply_force(PhysicsObjectHandle h, elysia::core::Vector2 f,
                               std::optional<elysia::core::Vector2> point)
{
    auto &p = *_impl;
    if (!p.get(h) || !finite(f) || (point && !finite(*point)))
        return false;
    p.enqueue([&p, h, f, point] {
        if (auto *o = p.raw(h))
        {
            if (point)
                b2Body_ApplyForce(o->native, p.to(f), p.to(*point), true);
            else
                b2Body_ApplyForceToCenter(o->native, p.to(f), true);
        }
    });
    return true;
}
bool PhysicsWorld::apply_impulse(PhysicsObjectHandle h, elysia::core::Vector2 f,
                                 std::optional<elysia::core::Vector2> point)
{
    auto &p = *_impl;
    if (!p.get(h) || !finite(f) || (point && !finite(*point)))
        return false;
    p.enqueue([&p, h, f, point] {
        if (auto *o = p.raw(h))
        {
            if (point)
                b2Body_ApplyLinearImpulse(o->native, p.to(f), p.to(*point), true);
            else
                b2Body_ApplyLinearImpulseToCenter(o->native, p.to(f), true);
        }
    });
    return true;
}
bool PhysicsWorld::apply_torque(PhysicsObjectHandle h, float f)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(f))
        return false;
    p.enqueue([&p, h, f] {
        if (auto *o = p.raw(h))
            b2Body_ApplyTorque(o->native, p.units.to_squared(f), true);
    });
    return true;
}
bool PhysicsWorld::apply_angular_impulse(PhysicsObjectHandle h, float f)
{
    auto &p = *_impl;
    if (!p.get(h) || !std::isfinite(f))
        return false;
    p.enqueue([&p, h, f] {
        if (auto *o = p.raw(h))
            b2Body_ApplyAngularImpulse(o->native, p.units.to_squared(f), true);
    });
    return true;
}
bool PhysicsWorld::teleport_object(PhysicsObjectHandle h, elysia::core::Vector2 pos,
                                   TeleportVelocityMode mode)
{
    auto &p = *_impl;
    if (!p.get(h) || !finite(pos))
        return false;
    p.enqueue([&p, h, pos, mode] {
        if (auto *o = p.raw(h))
            p.transform(*o, {pos, o->current.angle}, mode);
    });
    return true;
}
void PhysicsWorld::Impl::transform(Object &o, PhysicsPose pose, TeleportVelocityMode mode)
{
    for (auto id : o.shapes)
        cache.invalidate_target(CollisionTarget::from_collider(id));
    b2Body_SetTransform(o.native, to(pose.position), b2MakeRot(pose.angle));
    b2Body_SetAwake(o.native, true);
    o.previous = o.current = pose;
    o.owner->set_position(pose.position);
    if (mode == TeleportVelocityMode::Clear)
    {
        b2Body_SetLinearVelocity(o.native, b2Vec2_zero);
        b2Body_SetAngularVelocity(o.native, 0);
    }
}
bool PhysicsWorld::set_transform(PhysicsObjectHandle h, PhysicsPose pose, TeleportVelocityMode mode)
{
    auto &p = *_impl;
    if (!p.get(h) || !finite(pose.position) || !std::isfinite(pose.angle))
        return false;
    p.enqueue([&p, h, pose, mode] {
        if (auto *o = p.raw(h))
            p.transform(*o, pose, mode);
    });
    return true;
}
bool PhysicsWorld::update_collider(ColliderId id, const Collider &c)
{
    auto &p = *_impl;
    if (!contains_collider(id))
        return false;
    if (const auto failure = collider_validation_failure(c))
    {
        const auto key = std::pair{id, static_cast<std::uint8_t>(*failure)};
        if (p.logged_invalid_collider_updates.insert(key).second)
            log_invalid_collider_update(id, *failure, c);
        return false;
    }
    p.enqueue([&p, id, c] {
        auto it = p.shapes.find(id);
        if (it == p.shapes.end())
            return;
        auto &s = it->second;
        if (s.definition == c)
            return;
        p.retire(s);
        s.definition = c;
        if (auto *o = p.raw(s.owner))
        {
            p.create_shape(s, o->native);
            p.mass(*o);
            b2Body_SetBullet(o->native, p.bullet(*o));
        }
    });
    return true;
}
bool PhysicsWorld::set_collider_enabled(ColliderId id, bool enabled)
{
    auto &p = *_impl;
    if (!contains_collider(id))
        return false;
    p.enqueue([&p, id, enabled] {
        auto it = p.shapes.find(id);
        if (it == p.shapes.end())
            return;
        auto &s = it->second;
        if (s.definition.enabled == enabled)
            return;
        p.retire(s);
        s.definition.enabled = enabled;
        if (auto *o = p.raw(s.owner))
        {
            p.create_shape(s, o->native);
            p.mass(*o);
        }
    });
    return true;
}
bool PhysicsWorld::add_listener(ICollisionListener &l)
{
    auto &p = *_impl;
    p.enqueue([&p, &l] {
        if (std::ranges::find(p.listeners, &l) == p.listeners.end())
            p.listeners.push_back(&l);
    });
    return true;
}
bool PhysicsWorld::remove_listener(const ICollisionListener &l)
{
    auto &p = *_impl;
    p.enqueue([&p, &l] { std::erase(p.listeners, &l); });
    return true;
}
void PhysicsWorld::collect_contacts(CollisionTarget t, std::vector<CollisionContact> &out) const
{
    _impl->cache.collect_contacts(t, out);
}
PhysicsContactState PhysicsWorld::contact_state(CollisionTarget t) const noexcept
{
    PhysicsContactState s;
    for (auto &c : _impl->cache.contacts())
    {
        if (c.response != CollisionResponse::Block)
            continue;
        auto n = c.manifold.normal;
        if (c.pair.second == t)
            n = -n;
        else if (c.pair.first != t)
            continue;
        float e = config().contact_normal_threshold;
        s.grounded |= n.y > e;
        s.ceiling |= n.y < -e;
        s.wall_right |= n.x > e;
        s.wall_left |= n.x < -e;
    }
    return s;
}
PhysicsContactState PhysicsWorld::contact_state(PhysicsObjectHandle h) const noexcept
{
    PhysicsContactState s;
    if (auto *o = _impl->get(h))
        for (auto id : o->shapes)
        {
            auto c = contact_state(CollisionTarget::from_collider(id));
            s.grounded |= c.grounded;
            s.ceiling |= c.ceiling;
            s.wall_left |= c.wall_left;
            s.wall_right |= c.wall_right;
        }
    return s;
}
bool PhysicsWorld::request_pass_through(ColliderId actor, CollisionTarget support)
{
    if (!contains_collider(actor) || !support.is_valid())
        return false;
    auto &p = *_impl;
    const Impl::Shape *platform = nullptr;
    if (support.kind == CollisionTargetKind::Tile)
    {
        auto it = p.tile_shapes.find(support.tile);
        if (it != p.tile_shapes.end())
            platform = &it->second;
    }
    else
    {
        auto it = p.shapes.find(support.collider);
        if (it != p.shapes.end())
            platform = &it->second;
    }
    if (!platform || !platform->definition.one_way ||
        platform->definition.one_way->pass_through == PassThroughDirection::None)
        return false;
    p.enqueue([&p, actor, support] {
        p.ignored.insert(normalized_collision_pair(CollisionTarget::from_collider(actor), support));
        auto it = p.shapes.find(actor);
        if (it != p.shapes.end())
            if (auto *object = p.get(it->second.owner); object && B2_IS_NON_NULL(object->native))
                b2Body_SetAwake(object->native, true);
    });
    return true;
}
void PhysicsWorld::Impl::prepare_snapshot()
{
    snapshot.clear();
    previous_blocking_contacts.clear();
    for (const auto &contact : cache.contacts())
        if (contact.response == CollisionResponse::Block)
            previous_blocking_contacts.insert(contact.pair);
    auto add = [&](Shape &s) {
        if (B2_IS_NULL(s.native))
            return;
        auto body = b2Shape_GetBody(s.native);
        auto transform = b2Body_GetTransform(body);
        auto previous_transform = transform;
        if (auto *object = get(s.owner))
            previous_transform = {to(object->previous.position), b2MakeRot(object->previous.angle)};
        b2AABB bounds;
        b2AABB previous_bounds;
        if (b2Shape_GetType(s.native) == b2_circleShape)
        {
            auto circle = b2Shape_GetCircle(s.native);
            bounds = b2ComputeCircleAABB(&circle, transform);
            previous_bounds = b2ComputeCircleAABB(&circle, previous_transform);
        }
        else
        {
            auto polygon = b2Shape_GetPolygon(s.native);
            bounds = b2ComputePolygonAABB(&polygon, transform);
            previous_bounds = b2ComputePolygonAABB(&polygon, previous_transform);
        }
        unsigned internal_faces = 0;
        if (s.target.kind == CollisionTargetKind::Tile && !s.definition.one_way && tiles &&
            s.target.tile.x >= 0 && s.target.tile.x < tiles->columns() && s.target.tile.y >= 0 &&
            s.target.tile.y < tiles->rows())
        {
            const auto c = s.target.tile;
            const TileCoordinate neighbors[] = {
                {c.x - 1, c.y}, {c.x + 1, c.y}, {c.x, c.y - 1}, {c.x, c.y + 1}};
            for (unsigned i = 0; i < 4; ++i)
            {
                auto neighbor = tile_shapes.find(neighbors[i]);
                if (neighbor != tile_shapes.end() && B2_IS_NON_NULL(neighbor->second.native) &&
                    neighbor->second.definition.response == CollisionResponse::Block &&
                    !neighbor->second.definition.one_way &&
                    neighbor->second.definition.filter == s.definition.filter)
                    internal_faces |= 1u << i;
            }
        }
        snapshot.emplace(b2StoreShapeId(s.native),
                         Snapshot{s.target, s.definition.one_way, bounds, previous_bounds,
                                  b2Body_GetLinearVelocity(body), internal_faces});
    };
    for (auto &[id, s] : shapes)
        add(s);
    for (auto &[coord, s] : tile_shapes)
        add(s);
    std::erase_if(ignored, [&](const CollisionPair &pair) {
        const Snapshot *a = nullptr;
        const Snapshot *b = nullptr;
        for (auto &[id, s] : snapshot)
        {
            if (s.target == pair.first)
                a = &s;
            if (s.target == pair.second)
                b = &s;
        }
        if (!a || !b)
            return true;
        // Resting Box2D shapes have a small separation. Keep the request across that
        // gap until the actor has moved clear of the platform.
        const float margin =
            std::max({0.02f, a->one_way ? units.to_length(a->one_way->tolerance) : 0.0f,
                      b->one_way ? units.to_length(b->one_way->tolerance) : 0.0f});
        return a->bounds.upperBound.x + margin < b->bounds.lowerBound.x ||
               b->bounds.upperBound.x + margin < a->bounds.lowerBound.x ||
               a->bounds.upperBound.y + margin < b->bounds.lowerBound.y ||
               b->bounds.upperBound.y + margin < a->bounds.lowerBound.y;
    });
}
bool PhysicsWorld::Impl::pre_solve(b2ShapeId a, b2ShapeId b, b2Manifold *manifold, void *context)
{
    const auto &p = *static_cast<const Impl *>(context);
    auto ai = p.snapshot.find(b2StoreShapeId(a)), bi = p.snapshot.find(b2StoreShapeId(b));
    if (ai == p.snapshot.end() || bi == p.snapshot.end())
        return true;
    auto &x = ai->second;
    auto &y = bi->second;
    if (p.ignored.contains(normalized_collision_pair(x.target, y.target)))
        return false;
    const bool supported =
        p.previous_blocking_contacts.contains(normalized_collision_pair(x.target, y.target));
    auto allow = [&](const Snapshot &platform, const Snapshot &actor, b2Vec2 normal) {
        // Per-cell identity is retained, but shared solid faces are not terrain surfaces.
        const unsigned face = std::abs(normal.x) > std::abs(normal.y) ? (normal.x < 0 ? 1u : 2u)
                                                                      : (normal.y < 0 ? 4u : 8u);
        if (platform.internal_faces & face)
            return false;
        if (!platform.one_way)
            return true;
        auto rules = platform.one_way->pass_through;
        float tol = std::max(0.005f, p.units.to_length(platform.one_way->tolerance));
        // A discrete contact can first arrive one step after crossing the surface.
        // Preserve that approach side using the preceding pose, including moving supports.
        // Never query or mutate the world here.
        bool has_direction = false;
        bool blocks = false;
        if (has_pass_through_direction(rules, PassThroughDirection::Up))
        {
            has_direction = true;
            blocks |=
                (supported || actor.bounds.upperBound.y <= platform.bounds.lowerBound.y + tol ||
                 actor.previous_bounds.upperBound.y <=
                     platform.previous_bounds.lowerBound.y + tol) &&
                normal.y < -0.5f;
        }
        if (has_pass_through_direction(rules, PassThroughDirection::Down))
        {
            has_direction = true;
            blocks |=
                (supported || actor.bounds.lowerBound.y >= platform.bounds.upperBound.y - tol ||
                 actor.previous_bounds.lowerBound.y >=
                     platform.previous_bounds.upperBound.y - tol) &&
                normal.y > 0.5f;
        }
        if (has_pass_through_direction(rules, PassThroughDirection::Left))
        {
            has_direction = true;
            blocks |=
                (supported || actor.bounds.upperBound.x <= platform.bounds.lowerBound.x + tol ||
                 actor.previous_bounds.upperBound.x <=
                     platform.previous_bounds.lowerBound.x + tol) &&
                normal.x < -0.5f;
        }
        if (has_pass_through_direction(rules, PassThroughDirection::Right))
        {
            has_direction = true;
            blocks |=
                (supported || actor.bounds.lowerBound.x >= platform.bounds.upperBound.x - tol ||
                 actor.previous_bounds.lowerBound.x >=
                     platform.previous_bounds.upperBound.x - tol) &&
                normal.x > 0.5f;
        }
        return !has_direction || blocks;
    };
    return allow(x, y, manifold->normal) && allow(y, x, b2Neg(manifold->normal));
}
void PhysicsWorld::Impl::collect(std::vector<CollisionContact> &contacts)
{
    std::map<CollisionPair, CollisionContact> result;
    for (auto &[id, o] : objects)
    {
        if (o.removed || B2_IS_NULL(o.native) || !b2Body_IsEnabled(o.native))
            continue;
        std::vector<b2ContactData> data(b2Body_GetContactCapacity(o.native));
        int n = b2Body_GetContactData(o.native, data.data(), int(data.size()));
        for (int i = 0; i < n; ++i)
        {
            auto &d = data[i];
            auto a = target(d.shapeIdA), b = target(d.shapeIdB);
            if (!a || !b || !d.manifold.pointCount)
                continue;
            auto pair = normalized_collision_pair(*a, *b);
            if (ignored.contains(pair))
                continue;
            if (!pre_solve(d.shapeIdA, d.shapeIdB, &d.manifold, this))
                continue;
            CollisionContact c;
            c.pair = pair;
            c.response = CollisionResponse::Block;
            c.manifold.normal = {d.manifold.normal.x, d.manifold.normal.y};
            if (pair.first != *a)
                c.manifold.normal = -c.manifold.normal;
            c.manifold.contact_point_count = std::uint8_t(d.manifold.pointCount);
            for (int j = 0; j < d.manifold.pointCount; ++j)
            {
                auto &point = d.manifold.points[j];
                c.manifold.contact_points[j] = from(point.point);
                c.manifold.penetration =
                    std::max(c.manifold.penetration, units.from_length(-point.separation));
                c.normal_impulse += units.from_length(point.normalImpulse);
                c.tangent_impulse += units.from_length(point.tangentImpulse);
            }
            result[pair] = c;
        }
    }
    auto sensors = [&](const auto &entries) {
        for (auto &[id, s] : entries)
        {
            if (B2_IS_NULL(s.native) || !b2Shape_IsSensor(s.native))
                continue;
            std::vector<b2ShapeId> hits(b2Shape_GetSensorCapacity(s.native));
            int n = b2Shape_GetSensorOverlaps(s.native, hits.data(), int(hits.size()));
            for (int i = 0; i < n; ++i)
            {
                auto t = target(hits[i]);
                if (!t)
                    continue;
                auto pair = normalized_collision_pair(s.target, *t);
                CollisionContact c;
                c.pair = pair;
                c.response = CollisionResponse::Overlap;
                result[pair] = c;
            }
        }
    };
    sensors(shapes);
    sensors(tile_shapes);
    contacts.clear();
    for (auto &[pair, c] : result)
        contacts.push_back(c);
    // Consume native event IDs while tombstones are still alive. Only value-semantic
    // contacts leave this layer; cache invalidations supply exactly one logical End.
    auto se = b2World_GetSensorEvents(world);
    for (int i = 0; i < se.endCount; ++i)
    {
        (void)target(se.endEvents[i].sensorShapeId);
        (void)target(se.endEvents[i].visitorShapeId);
    }
    auto ce = b2World_GetContactEvents(world);
    for (int i = 0; i < ce.endCount; ++i)
    {
        (void)target(ce.endEvents[i].shapeIdA);
        (void)target(ce.endEvents[i].shapeIdB);
    }
    std::erase_if(mapping,
                  [&](auto &v) { return v.second.retire_after && v.second.retire_after <= epoch; });
}
void PhysicsWorld::step(double fixed_delta_seconds)
{
    auto &p = *_impl;
    if (!std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
        throw std::invalid_argument("PhysicsWorld::step requires a finite positive delta.");
    if (p.advancing)
        throw std::logic_error("PhysicsWorld::step cannot be called reentrantly.");

    p.advancing = true;
    try
    {
        const auto start = std::chrono::steady_clock::now();
        const bool reset_before_step = p.pending_reset;
        p.flush();
        if (!reset_before_step)
        {
            std::vector<std::uint64_t> participants;
            for (auto &[id, o] : p.objects)
                participants.push_back(id);
            for (auto id : participants)
            {
                auto &o = p.objects.at(id);
                if (p.pending_reset)
                    break;
                if (!o.removed && o.owner->is_active() && !o.owner->is_destroyed())
                    if (auto *participant = dynamic_cast<PhysicsStepParticipant *>(o.owner))
                        participant->fixed_update(fixed_delta_seconds);
            }
            bool reset = p.pending_reset;
            p.flush();
            if (!reset)
            {
                std::vector<std::uint64_t> dead;
                for (auto &[id, o] : p.objects)
                {
                    if (o.owner->is_destroyed())
                    {
                        dead.push_back(id);
                        continue;
                    }
                    const bool enabled = o.definition.enabled && o.owner->is_active();
                    if (enabled != b2Body_IsEnabled(o.native))
                    {
                        if (enabled)
                            b2Body_Enable(o.native);
                        else
                            b2Body_Disable(o.native);
                    }
                }
                for (const auto id : dead)
                    p.destroy_object(id);
                p.prepare_snapshot();
                for (auto &[id, o] : p.objects)
                    o.previous = o.current;
                b2World_Step(p.world, static_cast<float>(fixed_delta_seconds),
                             static_cast<int>(p.config.sub_steps));
                ++p.epoch;
                std::vector<CollisionContact> contacts;
                p.collect(contacts);
                std::vector<CollisionEvent> events;
                p.cache.update(contacts, events);
                for (auto &[id, o] : p.objects)
                {
                    o.current = {p.from(b2Body_GetPosition(o.native)),
                                 b2Rot_GetAngle(b2Body_GetRotation(o.native))};
                    o.owner->set_position(o.current.position);
                }
                p.stats.registered_objects = registered_object_count();
                p.stats.registered_colliders = registered_collider_count();
                p.stats.contacts = contacts.size();
                const auto listeners = p.listeners;
                for (const auto &event : events)
                    for (auto *listener : listeners)
                        listener->on_collision_event(event);
                reset = p.pending_reset;
                p.flush();
                if (!reset)
                {
                    p.stats.step_milliseconds =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start)
                            .count();
                    p.stats.awake_bodies = b2World_GetAwakeBodyCount(p.world);
                    p.stats.joints = p.joints.size();
                }
            }
        }
        p.advancing = false;
    }
    catch (...)
    {
        p.commands.clear();
        p.rollback_aborted_registrations();
        p.advancing = false;
        throw;
    }
}

void PhysicsWorld::finalize_frame(double interpolation_alpha)
{
    auto &p = *_impl;
    p.interpolation_alpha = std::clamp(interpolation_alpha, 0.0, 1.0);
    p.capture_debug(p.capture);
    p.debug.interpolation_alpha = static_cast<float>(p.interpolation_alpha);
    for (auto &[id, object] : p.objects)
    {
        const auto pose = render_pose({id});
        if (pose)
            object.owner->_render_offset = pose->position - object.current.position;
    }
}
void PhysicsWorld::set_debug_capture(PhysicsDebugCapture c)
{
    constexpr auto valid_bits = static_cast<std::uint8_t>(PhysicsDebugCapture::All);
    c = static_cast<PhysicsDebugCapture>(static_cast<std::uint8_t>(c) & valid_bits);
    if (_impl->capture == c)
        return;
    _impl->capture_debug(c);
    _impl->capture = c;
    _impl->debug.interpolation_alpha = static_cast<float>(_impl->interpolation_alpha);
}
PhysicsDebugCapture PhysicsWorld::debug_capture() const noexcept
{
    return _impl->capture;
}
const PhysicsDebugSnapshot &PhysicsWorld::debug_snapshot() const noexcept
{
    return _impl->debug;
}
} // namespace elysia::physics
