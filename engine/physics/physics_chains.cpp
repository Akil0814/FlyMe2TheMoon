#include "detail/physics_world_impl.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace elysia::physics
{
namespace
{
bool finite(b2Vec2 p)
{
    return b2IsValidVec2(p);
}
bool intersects(b2Vec2 a, b2Vec2 b, b2Vec2 c, b2Vec2 d)
{
    auto cross = [](b2Vec2 a, b2Vec2 b, b2Vec2 c) {
        return double(b.x - a.x) * double(c.y - a.y) - double(b.y - a.y) * double(c.x - a.x);
    };
    auto contains = [](b2Vec2 a, b2Vec2 b, b2Vec2 p) {
        return p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) && p.y >= std::min(a.y, b.y) &&
               p.y <= std::max(a.y, b.y);
    };
    double x = cross(a, b, c), y = cross(a, b, d), z = cross(c, d, a), w = cross(c, d, b);
    if (x == 0 && contains(a, b, c))
        return true;
    if (y == 0 && contains(a, b, d))
        return true;
    if (z == 0 && contains(c, d, a))
        return true;
    if (w == 0 && contains(c, d, b))
        return true;
    return (x > 0) != (y > 0) && (z > 0) != (w > 0);
}
bool chain_points(const ChainDefinition &d, const detail::PhysicsUnits &units, std::vector<b2Vec2> &out)
{
    const auto n = d.local_vertices.size();
    if (n < (d.loop ? 4u : 2u) || n > std::size_t(std::numeric_limits<int>::max() - 2) ||
        !std::isfinite(d.material.friction) || d.material.friction < 0 || !std::isfinite(d.material.restitution) ||
        d.material.restitution < 0 || (d.loop && (d.ghost_before || d.ghost_after)))
        return false;
    const auto to = [&](elysia::core::Vector2 p) {
        p = units.to_length(p);
        return b2Vec2{p.x, p.y};
    };
    out.reserve(n + 2);
    if (!d.loop)
        out.push_back(to(d.ghost_before.value_or(d.local_vertices[0] * 2 - d.local_vertices[1])));
    for (auto p : d.local_vertices)
    {
        auto v = to(p);
        if (!finite(v))
            return false;
        out.push_back(v);
    }
    if (!d.loop)
        out.push_back(to(d.ghost_after.value_or(d.local_vertices[n - 1] * 2 - d.local_vertices[n - 2])));
    for (auto p : out)
        if (!finite(p))
            return false;
    const auto edge_valid = [](b2Vec2 a, b2Vec2 b) {
        float length = b2Distance(a, b);
        return std::isfinite(length) && length >= .005f;
    };
    for (std::size_t i = 1; i < out.size(); ++i)
        if (!edge_valid(out[i - 1], out[i]))
            return false;
    if (d.loop && !edge_valid(out.back(), out.front()))
        return false;
    const std::size_t offset = d.loop ? 0 : 1, edges = d.loop ? n : n - 1;
    // Reject repeated non-adjacent vertices and intersections, preserving the user's winding.
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            if (!edge_valid(out[offset + i], out[offset + j]))
                return false;
    for (std::size_t i = 0; i < edges; ++i)
        for (std::size_t j = i + 1; j < edges; ++j)
        {
            if (j == i + 1 || (d.loop && i == 0 && j == edges - 1))
                continue;
            if (intersects(out[offset + i], out[offset + (i + 1) % n], out[offset + j], out[offset + (j + 1) % n]))
                return false;
        }
    // An immediate reversal would overlap adjacent edges and make invalid ghost geometry.
    for (std::size_t i = 1; i + 1 < out.size(); ++i)
    {
        auto a = b2Sub(out[i], out[i - 1]), b = b2Sub(out[i + 1], out[i]);
        if (b2Cross(a, b) == 0 && b2Dot(a, b) < 0)
            return false;
    }
    if (d.loop)
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto a = b2Sub(out[i], out[(i + n - 1) % n]), b = b2Sub(out[(i + 1) % n], out[i]);
            if (b2Cross(a, b) == 0 && b2Dot(a, b) < 0)
                return false;
        }
    return true;
}
} // namespace
void PhysicsWorld::Impl::destroy_chain_record(std::uint64_t id, bool invalidate_contacts, bool discard_mapping)
{
    auto it = chains.find(id);
    if (it == chains.end())
        return;
    auto &chain = *it->second;
    if (invalidate_contacts)
        cache.invalidate_targets(chain.targets);
    for (auto collider : chain.colliders)
    {
        auto s = shapes.find(collider);
        if (s == shapes.end())
            continue;
        if (B2_IS_NON_NULL(s->second.native))
        {
            auto native = b2StoreShapeId(s->second.native);
            if (discard_mapping)
                mapping.erase(native);
            else if (auto m = mapping.find(native); m != mapping.end())
                m->second.retire_after = epoch + 1;
        }
        shapes.erase(s);
    }
    if (B2_IS_NON_NULL(chain.native))
        b2DestroyChain(chain.native);
    if (auto *owner = get(chain.owner); owner && B2_IS_NON_NULL(owner->native))
        mass(*owner);
    chains.erase(it);
}
ChainHandle PhysicsWorld::create_chain(PhysicsObjectHandle owner, const ChainDefinition &definition)
{
    auto &p = *_impl;
    auto *object = p.get(owner);
    if (!object || object->definition.type == BodyType::Dynamic ||
        p.next_chain == std::numeric_limits<std::uint64_t>::max())
        return {};
    auto record = std::make_shared<Impl::Chain>();
    record->owner = owner;
    record->definition = definition;
    if (!chain_points(definition, p.units, record->points))
        return {};
    const auto count = definition.loop ? definition.local_vertices.size() : definition.local_vertices.size() - 1;
    if (count >= std::numeric_limits<std::uint64_t>::max() - p.next_shape)
        return {};
    record->colliders.reserve(count);
    record->targets.reserve(count);
    record->native_segments.resize(count);
    ChainHandle handle{p.next_chain++};
    p.chains.emplace(handle.value, record);
    try
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            ColliderId id = p.next_shape++;
            record->colliders.push_back(id);
            const auto target = CollisionTarget::from_collider(id);
            record->targets.push_back(target);
            Collider collider;
            collider.shape = SegmentShape{definition.local_vertices[i],
                                          definition.local_vertices[(i + 1) % definition.local_vertices.size()]};
            collider.filter = definition.filter;
            collider.material = definition.material;
            collider.tag = record->definition.tag;
            collider.density.kilograms_per_square_meter = 0;
            p.shapes.emplace(id, Impl::Shape{{}, owner, target, collider, handle});
        }
        p.enqueue([&p, handle] {
            auto i = p.chains.find(handle.value);
            if (i == p.chains.end() || i->second->removed)
                return;
            auto &chain = *i->second;
            auto *owner = p.get(chain.owner);
            if (!owner || B2_IS_NULL(owner->native))
            {
                p.destroy_chain_record(handle.value, false, true);
                return;
            }
            try
            {
                auto def = b2DefaultChainDef();
                auto material = b2DefaultSurfaceMaterial();
                material.friction = chain.definition.material.friction;
                material.restitution = chain.definition.material.restitution;
                def.points = chain.points.data();
                def.count = int(chain.points.size());
                def.isLoop = chain.definition.loop;
                def.materials = &material;
                def.materialCount = 1;
                def.enableSensorEvents = true;
                def.filter.categoryBits = chain.definition.filter.category;
                def.filter.maskBits = chain.definition.filter.mask;
                def.filter.groupIndex = chain.definition.filter.group;
                chain.native = b2CreateChain(owner->native, &def);
                p.mass(*owner);
                b2Chain_GetSegments(chain.native, chain.native_segments.data(), int(chain.native_segments.size()));
                for (std::size_t n = 0; n < chain.colliders.size(); ++n)
                {
                    auto &shape = p.shapes.at(chain.colliders[n]);
                    shape.native = chain.native_segments[n];
                    b2Shape_EnableContactEvents(shape.native, true);
                    b2Shape_EnablePreSolveEvents(shape.native, true);
                    b2Shape_EnableSensorEvents(shape.native, true);
                    p.mapping.emplace(b2StoreShapeId(shape.native), Impl::Mapping{shape.target, 0});
                }
            }
            catch (...)
            {
                p.destroy_chain_record(handle.value, false, true);
                throw;
            }
        });
    }
    catch (...)
    {
        p.destroy_chain_record(handle.value, false, true);
        throw;
    }
    return handle;
}
bool PhysicsWorld::destroy_chain(ChainHandle h)
{
    auto &p = *_impl;
    auto i = p.chains.find(h.value);
    if (i == p.chains.end() || i->second->removed || !p.get(i->second->owner))
        return false;
    auto record = i->second;
    const bool deferred = p.advancing;
    p.enqueue([&p, h] { p.destroy_chain_record(h.value, true); });
    if (deferred)
        record->removed = true;
    return true;
}
std::optional<ChainState> PhysicsWorld::chain_state(ChainHandle h) const noexcept
{
    auto &p = *_impl;
    auto i = p.chains.find(h.value);
    if (i == p.chains.end() || i->second->removed || !p.get(i->second->owner) || B2_IS_NULL(i->second->native))
        return {};
    auto &chain = *i->second;
    return ChainState{chain.owner, chain.definition.loop, chain.definition.local_vertices.size(),
                      chain.colliders.size()};
}
ColliderId PhysicsWorld::chain_collider(ChainHandle h, std::size_t index) const noexcept
{
    if (!chain_state(h))
        return InvalidColliderId;
    const auto &ids = _impl->chains.at(h.value)->colliders;
    return index < ids.size() ? ids[index] : InvalidColliderId;
}
std::optional<ChainSegmentInfo> PhysicsWorld::chain_segment_info(ColliderId id) const noexcept
{
    if (!contains_collider(id))
        return {};
    const auto &shape = _impl->shapes.at(id);
    if (!shape.chain.is_valid())
        return {};
    const auto &chain = *_impl->chains.at(shape.chain.value);
    auto i = std::ranges::find(chain.colliders, id);
    return ChainSegmentInfo{shape.chain, shape.owner, std::size_t(i - chain.colliders.begin())};
}
} // namespace elysia::physics
