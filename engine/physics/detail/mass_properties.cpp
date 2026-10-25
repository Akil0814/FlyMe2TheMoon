#include "mass_properties.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace elysia::physics::detail
{
namespace
{
bool finite(float v)
{
    return std::isfinite(v);
}
bool finite(b2Vec2 v)
{
    return finite(v.x) && finite(v.y);
}
bool positive(float v)
{
    return finite(v) && v > 0 && finite(1.f / v) && 1.f / v > 0;
}
bool representable(double v)
{
    return std::isfinite(v) && std::abs(v) <= std::numeric_limits<float>::max();
}
} // namespace

void MassBuilder::add(const Collider &collider)
{
    if (!collider.enabled || collider.response == CollisionResponse::Ignore)
        return;
    const auto geometry = shape_geometry(collider.shape, _units);
    if (!geometry)
    {
        _valid = false;
        return;
    }
    for (int i = 0; i < geometry->proxy.count; ++i)
        _extent_radius = std::max(_extent_radius,
                                  std::hypot(double(geometry->proxy.points[i].x), double(geometry->proxy.points[i].y)) +
                                      geometry->proxy.radius);
    const float density = collider.density.kilograms_per_square_meter;
    if (density == 0 || (collider.response == CollisionResponse::Overlap && !collider.sensor_contributes_mass))
        return;
    b2MassData data{};
    switch (geometry->kind)
    {
    case ShapeGeometry::Kind::Circle:
        data = b2ComputeCircleMass(&geometry->circle, density);
        break;
    case ShapeGeometry::Kind::Capsule:
        data = b2ComputeCapsuleMass(&geometry->capsule, density);
        break;
    case ShapeGeometry::Kind::Polygon:
        data = b2ComputePolygonMass(&geometry->polygon, density);
        break;
    case ShapeGeometry::Kind::Segment:
        return;
    }
    if (!finite(data.mass) || !finite(data.center) || !finite(data.rotationalInertia) || data.mass <= 0)
    {
        _valid = false;
        return;
    }
    _mass += data.mass;
    _weighted_x += double(data.mass) * data.center.x;
    _weighted_y += double(data.mass) * data.center.y;
    _inertia += data.rotationalInertia;
}

std::optional<b2MassData> MassBuilder::finish(BodyType type, const MassProperties &properties,
                                              bool fixed_rotation) const
{
    if (properties.valueless_by_exception())
        return {};
    if (type != BodyType::Dynamic)
        return std::holds_alternative<DensityMass>(properties) ? std::optional{b2MassData{}} : std::nullopt;
    b2MassData out{};
    if (const auto *custom = std::get_if<BodyMassData>(&properties))
    {
        const auto center = _units.to_length(custom->local_center);
        out = {custom->mass, {center.x, center.y}, _units.to_squared(custom->rotational_inertia)};
        if (!positive(custom->mass) || !finite(custom->local_center.x) || !finite(custom->local_center.y) ||
            !finite(custom->rotational_inertia) || custom->rotational_inertia < 0 ||
            (custom->rotational_inertia > 0 && !positive(out.rotationalInertia)) ||
            (custom->local_center.x != 0 && center.x == 0) || (custom->local_center.y != 0 && center.y == 0))
            return {};
    }
    else
    {
        if (!_valid || !representable(_mass))
            return {};
        double cx = _mass > 0 ? _weighted_x / _mass : 0;
        double cy = _mass > 0 ? _weighted_y / _mass : 0;
        double inertia = _inertia - _mass * (cx * cx + cy * cy);
        if (!representable(cx) || !representable(cy) || !representable(inertia) || inertia < 0)
            return {};
        out = {float(_mass), {float(cx), float(cy)}, float(inertia)};
        if (const auto *explicit_mass = std::get_if<ExplicitMass>(&properties))
        {
            if (!positive(explicit_mass->mass))
                return {};
            inertia = _mass > 0 ? inertia * explicit_mass->mass / _mass : 0;
            if (!representable(inertia))
                return {};
            out.mass = explicit_mass->mass;
            out.rotationalInertia = float(inertia);
            if (inertia > 0 && out.rotationalInertia == 0)
                return {};
        }
        if ((out.mass > 0 && !positive(out.mass)) || (out.rotationalInertia > 0 && !positive(out.rotationalInertia)))
            return {};
    }
    // Extent calculations use squared distances, as do several native solvers.
    if (!finite(out.center) || !finite(b2LengthSquared(out.center)) ||
        _extent_radius + std::hypot(double(out.center.x), double(out.center.y)) >
            .5 * std::sqrt(double(std::numeric_limits<float>::max())) ||
        !finite(_units.from_squared(out.rotationalInertia)))
        return {};
    if (fixed_rotation)
        out.rotationalInertia = 0;
    return out;
}
} // namespace elysia::physics::detail
