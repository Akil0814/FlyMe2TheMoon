#include "shape_geometry.h"
#include <cmath>
#include <limits>
#include <type_traits>
namespace elysia::physics::detail
{
namespace
{
bool finite(elysia::core::Vector2 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}
bool finite(b2Vec2 p)
{
    return b2IsValidVec2(p);
}
} // namespace
std::optional<ShapeGeometry> shape_geometry(const ColliderShape &shape, const PhysicsUnits &units)
{
    auto to = [&](elysia::core::Vector2 p) {
        p = units.to_length(p);
        return b2Vec2{p.x, p.y};
    };
    return std::visit(
        [&](const auto &s) -> std::optional<ShapeGeometry> {
            using T = std::decay_t<decltype(s)>;
            ShapeGeometry g{};
            if constexpr (std::is_same_v<T, SegmentShape>)
            {
                if (!finite(s.local_first)||!finite(s.local_second)) return {};
                auto first=to(s.local_first),second=to(s.local_second);
                const float length=b2Distance(first,second);
                if (!finite(first)||!finite(second)||!std::isfinite(length)||length<.005f) return {};
                g.kind=ShapeGeometry::Kind::Segment;g.segment={first,second};
                const b2Vec2 points[]={first,second};g.proxy=b2MakeProxy(points,2,0);
                return g;
            }
            else if constexpr (std::is_same_v<T, CircleShape> || std::is_same_v<T, CapsuleShape>)
            {
                if (!std::isfinite(s.radius) || s.radius <= 0)
                    return {};
                float radius = units.to_length(s.radius);
                if (!std::isfinite(radius) || radius <= 0)
                    return {};
                if constexpr (std::is_same_v<T, CircleShape>)
                {
                    if (!finite(s.local_center) || !finite(to(s.local_center)))
                        return {};
                    g.kind = ShapeGeometry::Kind::Circle;
                    g.circle = {to(s.local_center), radius};
                    g.proxy = b2MakeProxy(&g.circle.center, 1, radius);
                }
                else
                {
                    if (!finite(s.local_first) || !finite(s.local_second) || !finite(to(s.local_first)) ||
                        !finite(to(s.local_second)))
                        return {};
                    b2Vec2 points[] = {to(s.local_first), to(s.local_second)};
                    const float length_squared = b2DistanceSquared(points[0], points[1]);
                    if (!std::isfinite(length_squared))
                        return {};
                    // Match the bundled native capsule constructor's linear-slop collapse.
                    if (length_squared <= 0.005f * 0.005f)
                    {
                        g.kind = ShapeGeometry::Kind::Circle;
                        g.circle = {b2Lerp(points[0], points[1], .5f), radius};
                        g.proxy = b2MakeProxy(&g.circle.center, 1, radius);
                    }
                    else
                    {
                        g.kind = ShapeGeometry::Kind::Capsule;
                        g.capsule = {points[0], points[1], radius};
                        g.proxy = b2MakeProxy(points, 2, radius);
                    }
                }
            }
            else
            {
                g.kind = ShapeGeometry::Kind::Polygon;
                if constexpr (std::is_same_v<T, BoxShape>)
                {
                    if (!finite(s.local_center) || !finite(s.half_extents) || s.half_extents.x <= 0 ||
                        s.half_extents.y <= 0 || !std::isfinite(s.local_angle))
                        return {};
                    auto half = to(s.half_extents), center = to(s.local_center);
                    if (!finite(half) || !finite(center) || half.x <= 0 || half.y <= 0)
                        return {};
                    g.polygon = b2MakeOffsetBox(half.x, half.y, center, b2MakeRot(s.local_angle));
                }
                else
                {
                    const int n = s.vertex_count;
                    if (n < 3 || n > int(kMaxPolygonVertices))
                        return {};
                    b2Vec2 points[kMaxPolygonVertices];
                    for (int i = 0; i < n; ++i)
                    {
                        if (!finite(s.local_vertices[i]) || !finite(to(s.local_vertices[i])))
                            return {};
                        points[i] = to(s.local_vertices[i]);
                    }
                    // Every non-edge vertex must lie strictly on the same side of each edge.
                    // This rejects concavity, self intersection, repeats and collinear vertices.
                    float winding = 0;
                    for (int i = 0; i < n; ++i)
                    {
                        auto edge = b2Sub(points[(i + 1) % n], points[i]);
                        for (int j = 0; j < n; ++j)
                        {
                            if (j == i || j == (i + 1) % n)
                                continue;
                            float cross = b2Cross(edge, b2Sub(points[j], points[i]));
                            if (!std::isfinite(cross) || cross == 0)
                                return {};
                            if (winding == 0)
                                winding = cross;
                            if ((cross > 0) != (winding > 0))
                                return {};
                        }
                    }
                    auto hull = b2ComputeHull(points, n);
                    if (hull.count != n || !b2ValidateHull(&hull))
                        return {};
                    float area = 0;
                    for (int i = 1; i + 1 < hull.count; ++i)
                        area += .5f * b2Cross(b2Sub(hull.points[i], hull.points[0]),
                                              b2Sub(hull.points[i + 1], hull.points[0]));
                    if (!std::isfinite(area) || area <= std::numeric_limits<float>::epsilon())
                        return {};
                    g.polygon = b2MakePolygon(&hull, 0);
                }
                g.proxy = b2MakeProxy(g.polygon.vertices, g.polygon.count, g.polygon.radius);
            }
            for (int i = 0; i < g.proxy.count; ++i)
                if (!finite(g.proxy.points[i]))
                    return {};
            b2MassData mass{};
            switch (g.kind)
            {
            case ShapeGeometry::Kind::Segment: return g;
            case ShapeGeometry::Kind::Circle:
                mass = b2ComputeCircleMass(&g.circle, 1);
                break;
            case ShapeGeometry::Kind::Capsule:
                mass = b2ComputeCapsuleMass(&g.capsule, 1);
                break;
            case ShapeGeometry::Kind::Polygon: {
                float area = 0;
                for (int i = 1; i + 1 < g.polygon.count; ++i)
                    area += .5f * b2Cross(b2Sub(g.polygon.vertices[i], g.polygon.vertices[0]),
                                          b2Sub(g.polygon.vertices[i + 1], g.polygon.vertices[0]));
                if (!std::isfinite(area) || area <= std::numeric_limits<float>::epsilon())
                    return {};
                mass = b2ComputePolygonMass(&g.polygon, 1);
                break;
            }
            }
            if (!std::isfinite(mass.mass) || mass.mass <= 0 || !finite(mass.center) ||
                !std::isfinite(mass.rotationalInertia))
                return {};
            return g;
        },
        shape);
}
b2AABB shape_bounds(b2ShapeId id, b2Transform transform)
{
    switch (b2Shape_GetType(id))
    {
    case b2_circleShape: {
        auto s = b2Shape_GetCircle(id);
        return b2ComputeCircleAABB(&s, transform);
    }
    case b2_segmentShape: {auto s=b2Shape_GetSegment(id);return b2ComputeSegmentAABB(&s,transform);}
    case b2_chainSegmentShape: {auto s=b2Shape_GetChainSegment(id);return b2ComputeSegmentAABB(&s.segment,transform);}
    case b2_capsuleShape: {
        auto s = b2Shape_GetCapsule(id);
        return b2ComputeCapsuleAABB(&s, transform);
    }
    default: {
        auto s = b2Shape_GetPolygon(id);
        return b2ComputePolygonAABB(&s, transform);
    }
    }
}
} // namespace elysia::physics::detail
