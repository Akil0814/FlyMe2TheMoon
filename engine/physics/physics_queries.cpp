#include "detail/physics_world_impl.h"
#include <algorithm>
#include <cmath>
namespace elysia::physics
{
void PhysicsWorld::Impl::query(const b2ShapeProxy &proxy, b2Vec2 translation,
                               const CollisionFilter &filter, bool sweep, bool ray,
                               std::vector<CollisionQueryHit> &out) const
{
    out.clear();
    struct Context
    {
        const Impl *p;
        const CollisionFilter *filter;
        std::vector<CollisionQueryHit> *out;
        float length;
        bool accept(b2ShapeId id, b2Vec2 point, b2Vec2 normal, float fraction)
        {
            auto target = p->target(id);
            if (!target)
                return false;
            const Shape *s = nullptr;
            if (target->kind == CollisionTargetKind::Collider)
            {
                auto i = p->shapes.find(target->collider);
                if (i == p->shapes.end() || !p->get(i->second.owner))
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
            const auto &f = s->definition.filter;
            bool allowed = (filter->group && filter->group == f.group)
                               ? filter->group > 0
                               : ((filter->mask & f.category) && (f.mask & filter->category));
            if (!allowed || !s->definition.enabled ||
                s->definition.response == CollisionResponse::Ignore)
                return false;
            out->push_back({*target,
                            p->from(point),
                            {normal.x, normal.y},
                            length * fraction,
                            fraction,
                            s->definition.response});
            return true;
        }
    } ctx{this, &filter, &out, units.from_length(b2Length(translation))};
    auto q = b2DefaultQueryFilter();
    q.categoryBits = ~std::uint64_t(0);
    q.maskBits = ~std::uint64_t(0);
    b2World_OverlapShape(
        world, &proxy, q,
        [](b2ShapeId id, void *raw) {
            auto &c = *static_cast<Context *>(raw);
            c.accept(id, b2Shape_GetAABB(id).lowerBound, b2Vec2_zero, 0);
            return true;
        },
        &ctx);
    if (sweep)
    {
        auto cast = [](b2ShapeId id, b2Vec2 point, b2Vec2 normal, float fraction,
                       void *raw) -> float {
            static_cast<Context *>(raw)->accept(id, point, normal, fraction);
            return 1;
        };
        if (ray)
            b2World_CastRay(world, proxy.points[0], translation, q, cast, &ctx);
        else
            b2World_CastShape(world, &proxy, translation, q, cast, &ctx);
    }
    std::ranges::sort(out, [](auto &a, auto &b) {
        return a.fraction != b.fraction ? a.fraction < b.fraction : a.target < b.target;
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
bool valid(const elysia::core::Rect &r)
{
    return finite(r.position()) && finite(r.size()) && r.width() > 0 && r.height() > 0;
}
b2ShapeProxy box_proxy(const elysia::core::Rect &r, const detail::PhysicsUnits &u)
{
    b2Vec2 points[] = {{u.to_length(r.x()), u.to_length(r.y())},
                       {u.to_length(r.x() + r.width()), u.to_length(r.y())},
                       {u.to_length(r.x() + r.width()), u.to_length(r.y() + r.height())},
                       {u.to_length(r.x()), u.to_length(r.y() + r.height())}};
    return b2MakeProxy(points, 4, 0);
}
} // namespace
void PhysicsWorld::raycast_all(const RayCastQuery &q, std::vector<CollisionQueryHit> &out) const
{
    out.clear();
    if (!finite(q.origin) || !finite(q.direction) || !std::isfinite(q.max_distance) ||
        q.max_distance < 0 || q.direction.is_zero())
        return;
    auto origin = _impl->to(q.origin);
    auto proxy = b2MakeProxy(&origin, 1, 0);
    _impl->query(proxy, _impl->to(q.direction.normalized() * q.max_distance), q.filter, true, true,
                 out);
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
void PhysicsWorld::segment_cast_all(const SegmentCastQuery &q,
                                    std::vector<CollisionQueryHit> &out) const
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
void PhysicsWorld::overlap_aabb(const AabbOverlapQuery &q,
                                std::vector<CollisionOverlapQueryHit> &out) const
{
    out.clear();
    if (!valid(q.bounds))
        return;
    std::vector<CollisionQueryHit> hits;
    _impl->query(box_proxy(q.bounds, _impl->units), b2Vec2_zero, q.filter, false, false, hits);
    for (auto &h : hits)
        out.push_back({h.target, h.response});
}
void PhysicsWorld::overlap_circle(const CircleOverlapQuery &q,
                                  std::vector<CollisionOverlapQueryHit> &out) const
{
    out.clear();
    if (!finite(q.center) || !std::isfinite(q.radius) || q.radius <= 0)
        return;
    auto center = _impl->to(q.center);
    auto proxy = b2MakeProxy(&center, 1, _impl->units.to_length(q.radius));
    std::vector<CollisionQueryHit> hits;
    _impl->query(proxy, b2Vec2_zero, q.filter, false, false, hits);
    for (auto &h : hits)
        out.push_back({h.target, h.response});
}
std::optional<CollisionQueryHit> PhysicsWorld::sweep_aabb(const AabbSweepQuery &q) const
{
    if (!valid(q.start_bounds) || !finite(q.displacement))
        return {};
    std::vector<CollisionQueryHit> hits;
    _impl->query(box_proxy(q.start_bounds, _impl->units), _impl->to(q.displacement), q.filter, true,
                 false, hits);
    return hits.empty() ? std::nullopt : std::optional(hits.front());
}
} // namespace elysia::physics
