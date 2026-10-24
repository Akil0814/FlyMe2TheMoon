#include "detail/physics_world_impl.h"
#include <algorithm>
#include <cmath>
namespace elysia::physics
{
void PhysicsWorld::Impl::query(const b2ShapeProxy &proxy, b2Vec2 translation, const CollisionQueryFilter &filter,
                               bool sweep, bool ray, elysia::core::Vector2 origin,
                               std::vector<CollisionQueryHit> &out) const
{
    out.clear();
    if (!b2IsValidVec2(translation) || !std::isfinite(b2Length(translation)))
        return;
    for (int i = 0; i < proxy.count; ++i)
        if (!b2IsValidVec2(proxy.points[i]))
            return;
    struct Context
    {
        const Impl *p;
        const CollisionQueryFilter *filter;
        elysia::core::Vector2 origin;
        std::vector<CollisionQueryHit> *out;
        float length;
        bool accept(b2ShapeId id, b2Vec2 point, b2Vec2 normal, float fraction, bool initial = false)
        {
            auto target = p->target(id);
            if (!target)
                return false;
            const Shape *s = nullptr;
            if (target->kind == CollisionTargetKind::Collider)
            {
                auto i = p->shapes.find(target->collider);
                if (i == p->shapes.end() || !p->get(i->second.owner) ||
                    (i->second.chain.is_valid() && (p->chains.at(i->second.chain.value)->removed || B2_IS_NULL(p->chains.at(i->second.chain.value)->native))))
                    return false;
                s = &i->second;
            }
            else
            {
                auto i = p->tile_shapes.find(target->tile);
                if (i == p->tile_shapes.end())
                    return false;
                s = &i->second;
            }
            if (filter->ignored_object.is_valid() && s->owner == filter->ignored_object)
                return false;
            if (!filter->include_sensors && s->definition.response == CollisionResponse::Overlap)
                return false;
            const auto &f = s->definition.filter;
            const auto *collision = &filter->collision;
            bool allowed = (collision->group && collision->group == f.group)
                               ? collision->group > 0
                               : ((collision->mask & f.category) && (f.mask & collision->category));
            if (!allowed || !s->definition.enabled || s->definition.response == CollisionResponse::Ignore)
                return false;
            out->push_back({*target, initial ? origin : p->from(point),
                            initial ? elysia::core::Vector2{} : elysia::core::Vector2{normal.x, normal.y},
                            length * fraction, fraction, s->definition.response, initial});
            return true;
        }
    } ctx{this, &filter, origin, &out, units.from_length(b2Length(translation))};
    auto q = b2DefaultQueryFilter();
    q.categoryBits = ~std::uint64_t(0);
    q.maskBits = ~std::uint64_t(0);
    b2World_OverlapShape(
        world, &proxy, q,
        [](b2ShapeId id, void *raw) {
            auto &c = *static_cast<Context *>(raw);
            c.accept(id, b2Vec2_zero, b2Vec2_zero, 0, true);
            return true;
        },
        &ctx);
    if (sweep && b2LengthSquared(translation) > 0)
    {
        auto cast = [](b2ShapeId id, b2Vec2 point, b2Vec2 normal, float fraction, void *raw) -> float {
            static_cast<Context *>(raw)->accept(id, point, normal, fraction);
            return 1;
        };
        if (ray)
            b2World_CastRay(world, proxy.points[0], translation, q, cast, &ctx);
        else
            b2World_CastShape(world, &proxy, translation, q, cast, &ctx);
    }
    // Native query filters have no group override. Recover forced-positive-group
    // targets with zero masks/categories using the same native narrow phase.
    if (filter.collision.group > 0)
    {
        auto forced = [&](const Shape &shape) {
            const auto &f = shape.definition.filter;
            if (f.group != filter.collision.group || (f.category != 0 && f.mask != 0) || B2_IS_NULL(shape.native) ||
                !b2Body_IsEnabled(b2Shape_GetBody(shape.native)))
                return;
            auto geometry = detail::shape_geometry(shape.definition.shape, units);
            if (!geometry)
                return;
            auto transform = b2Body_GetTransform(b2Shape_GetBody(shape.native));
            b2DistanceInput distance{};
            distance.proxyA = geometry->proxy;
            distance.proxyB = proxy;
            distance.transformA = transform;
            distance.transformB = b2Transform_identity;
            distance.useRadii = true;
            b2SimplexCache cache{};
            if (b2ShapeDistance(&distance, &cache, nullptr, 0).distance <= 0.0005f)
                ctx.accept(shape.native, b2Vec2_zero, b2Vec2_zero, 0, true);
            if (!sweep || b2LengthSquared(translation) == 0)
                return;
            b2CastOutput hit{};
            if (ray)
            {
                b2RayCastInput input{proxy.points[0], translation, 1};
                hit = b2Shape_RayCast(shape.native, &input);
            }
            else
            {
                b2ShapeCastPairInput input{};
                input.proxyA = geometry->proxy;
                input.proxyB = proxy;
                input.transformA = transform;
                input.transformB = b2Transform_identity;
                input.translationB = translation;
                input.maxFraction = 1;
                hit = b2ShapeCast(&input);
            }
            if (hit.hit)
                ctx.accept(shape.native, hit.point, hit.normal, hit.fraction);
        };
        for (const auto &[id, shape] : shapes)
            forced(shape);
        for (const auto &[id, shape] : tile_shapes)
            forced(shape);
    }
    std::ranges::sort(out, [](auto &a, auto &b) {
        if (a.fraction != b.fraction)
            return a.fraction < b.fraction;
        if (a.target != b.target)
            return a.target < b.target;
        return a.initial_overlap && !b.initial_overlap;
    });
    std::set<CollisionTarget> seen;
    std::erase_if(out, [&](auto &h) { return !seen.insert(h.target).second; });
}
namespace
{
bool finite(elysia::core::Vector2 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}
bool valid(PhysicsPose pose)
{
    return finite(pose.position) && std::isfinite(pose.angle);
}
b2ShapeProxy world_proxy(const detail::ShapeGeometry &geometry, PhysicsPose pose, const detail::PhysicsUnits &units)
{
    auto p = units.to_length(pose.position);
    return b2MakeOffsetProxy(geometry.proxy.points, geometry.proxy.count, geometry.proxy.radius, {p.x, p.y},
                             b2MakeRot(pose.angle));
}
} // namespace
void PhysicsWorld::raycast_all(const RayCastQuery &q, std::vector<CollisionQueryHit> &out) const
{
    out.clear();
    if (!finite(q.origin) || !finite(q.direction) || !std::isfinite(q.max_distance) || q.max_distance < 0 ||
        q.direction.is_zero() || !std::isfinite(q.direction.length()))
        return;
    auto origin = _impl->to(q.origin);
    auto proxy = b2MakeProxy(&origin, 1, 0);
    _impl->query(proxy, _impl->to(q.direction.normalized() * q.max_distance), q.filter, true, true, q.origin, out);
    for (auto &h : out)
        if (h.fraction == 0)
            h.point = q.origin;
}
std::optional<CollisionQueryHit> PhysicsWorld::raycast(const RayCastQuery &q) const
{
    std::vector<CollisionQueryHit> hits;
    raycast_all(q, hits);
    return hits.empty() ? std::nullopt : std::optional(hits.front());
}
void PhysicsWorld::segment_cast_all(const SegmentCastQuery &q, std::vector<CollisionQueryHit> &out) const
{
    auto d = q.end - q.start;
    raycast_all({q.start, d, d.length(), q.filter}, out);
}
std::optional<CollisionQueryHit> PhysicsWorld::segment_cast(const SegmentCastQuery &q) const
{
    std::vector<CollisionQueryHit> hits;
    segment_cast_all(q, hits);
    return hits.empty() ? std::nullopt : std::optional(hits.front());
}
void PhysicsWorld::overlap_shape(const ShapeOverlapQuery &q, std::vector<CollisionOverlapQueryHit> &out) const
{
    out.clear();
    if (std::holds_alternative<SegmentShape>(q.shape) || !valid(q.pose))
        return;
    auto geometry = detail::shape_geometry(q.shape, _impl->units);
    if (!geometry)
        return;
    std::vector<CollisionQueryHit> hits;
    _impl->query(world_proxy(*geometry, q.pose, _impl->units), b2Vec2_zero, q.filter, false, false, q.pose.position,
                 hits);
    for (const auto &h : hits)
        out.push_back({h.target, h.response});
}
void PhysicsWorld::shape_cast_all(const ShapeCastQuery &q, std::vector<CollisionQueryHit> &out) const
{
    out.clear();
    if (std::holds_alternative<SegmentShape>(q.shape) || !valid(q.pose) || !finite(q.displacement) || !std::isfinite(q.displacement.length()))
        return;
    auto geometry = detail::shape_geometry(q.shape, _impl->units);
    if (!geometry)
        return;
    _impl->query(world_proxy(*geometry, q.pose, _impl->units), _impl->to(q.displacement), q.filter, true, false,
                 q.pose.position, out);
}
std::optional<CollisionQueryHit> PhysicsWorld::shape_cast(const ShapeCastQuery &q) const
{
    std::vector<CollisionQueryHit> hits;
    shape_cast_all(q, hits);
    return hits.empty() ? std::nullopt : std::optional(hits.front());
}
} // namespace elysia::physics
