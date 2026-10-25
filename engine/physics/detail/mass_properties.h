#pragma once
#include "../collision/collider.h"
#include "shape_geometry.h"

namespace elysia::physics::detail
{
// Computes a candidate without changing native state. Shape inertia is about the
// local origin; the result supplied to the body setter is about its center of mass.
class MassBuilder
{
  public:
    explicit MassBuilder(const PhysicsUnits &units) : _units(units)
    {
    }
    void add(const Collider &);
    std::optional<b2MassData> finish(BodyType, const MassProperties &, bool fixed_rotation) const;

  private:
    const PhysicsUnits &_units;
    double _mass = 0, _weighted_x = 0, _weighted_y = 0, _inertia = 0;
    double _extent_radius = 0;
    bool _valid = true;
};
} // namespace elysia::physics::detail
