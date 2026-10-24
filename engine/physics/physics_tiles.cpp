#include "detail/physics_world_impl.h"
#include <algorithm>
#include <cmath>
namespace elysia::physics
{
void PhysicsWorld::Impl::build_tiles(TileCoordinate begin, TileCoordinate end)
{
    if (!tiles)
        return;
    auto size = tiles->tile_size(), origin = tiles->world_origin();
    for (int y = begin.y; y <= end.y; ++y)
        for (int x = begin.x; x <= end.x; ++x)
        {
            TileCoordinate coord{x, y};
            auto it = tile_shapes.find(coord);
            if (it != tile_shapes.end())
            {
                retire(it->second);
                tile_shapes.erase(it);
            }
            auto cell = tiles->cell_at(coord);
            if (cell.type == TileCollisionType::Empty)
                continue;
            Shape s;
            s.target = CollisionTarget::from_tile(coord);
            s.definition.shape =
                AabbShape{{origin.x + x * size.x, origin.y + y * size.y, size.x, size.y}};
            s.definition.filter = cell.filter;
            s.definition.material = cell.material;
            s.definition.tag = cell.tag;
            s.definition.response = cell.type == TileCollisionType::Overlap
                                        ? CollisionResponse::Overlap
                                        : CollisionResponse::Block;
            if (cell.type == TileCollisionType::OneWay)
                s.definition.one_way = cell.one_way;
            if (detail::collider_validation_failure(s.definition))
                continue;
            create_shape(s, tile_body);
            tile_shapes.emplace(coord, std::move(s));
        }
}
bool PhysicsWorld::set_tile_world(const ITileCollisionWorld &t)
{
    auto size = t.tile_size(), origin = t.world_origin();
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0 || size.y <= 0 ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) || t.columns() < 0 || t.rows() < 0 ||
        std::int64_t(t.columns()) * t.rows() > 1000000)
        return false;
    auto &p = *_impl;
    p.enqueue([&p, &t] {
        p.clear_tiles();
        p.tiles = &t;
        auto def = b2DefaultBodyDef();
        p.tile_body = b2CreateBody(p.world, &def);
        p.build_tiles({0, 0}, {t.columns() - 1, t.rows() - 1});
        if (t.out_of_bounds_policy() == TileOutOfBoundsPolicy::Block && t.columns() > 0 &&
            t.rows() > 0)
        {
            auto z = t.tile_size(), o = t.world_origin();
            float w = z.x * t.columns(), h = z.y * t.rows();
            float margin = std::max(w, h) + p.config.units_per_meter * 100;
            elysia::core::Rect walls[] = {{o.x - margin, o.y - margin, margin, h + 2 * margin},
                                          {o.x + w, o.y - margin, margin, h + 2 * margin},
                                          {o.x, o.y - margin, w, margin},
                                          {o.x, o.y + h, w, margin}};
            TileCoordinate ids[] = {{-1, 0}, {t.columns(), 0}, {0, -1}, {0, t.rows()}};
            for (int i = 0; i < 4; ++i)
            {
                Impl::Shape s;
                s.target = CollisionTarget::from_tile(ids[i]);
                s.definition.shape = AabbShape{walls[i]};
                p.create_shape(s, p.tile_body);
                p.tile_shapes.emplace(ids[i], std::move(s));
            }
        }
    });
    return true;
}
bool PhysicsWorld::clear_tile_world(const ITileCollisionWorld &t)
{
    auto &p = *_impl;
    if (p.tiles != &t && !p.advancing)
        return false;
    p.enqueue([&p, &t] {
        if (p.tiles == &t)
            p.clear_tiles();
    });
    return true;
}
bool PhysicsWorld::update_tiles(TileCoordinate begin, TileCoordinate end)
{
    auto &p = *_impl;
    if (!p.tiles || begin.x < 0 || begin.y < 0 || end.x < begin.x || end.y < begin.y ||
        end.x >= p.tiles->columns() || end.y >= p.tiles->rows())
        return false;
    p.enqueue([&p, begin, end] { p.build_tiles(begin, end); });
    return true;
}
const ITileCollisionWorld *PhysicsWorld::tile_world() const noexcept
{
    return _impl->tiles;
}
} // namespace elysia::physics
