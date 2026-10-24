#pragma once
#include "../../core/geometry/vector2.h"
#include <cmath>
#include <stdexcept>
namespace elysia::physics::detail
{
// The sole unit conversion boundary. Time, angles and mass are unchanged.
class PhysicsUnits
{
  public:
    explicit PhysicsUnits(float units_per_meter) : _scale(units_per_meter)
    {
        if (!std::isfinite(_scale) || _scale <= 0)
            throw std::invalid_argument("units_per_meter must be finite and positive");
    }
    float to_length(float v) const noexcept
    {
        return v / _scale;
    }
    float from_length(float v) const noexcept
    {
        return v * _scale;
    }
    float to_squared(float v) const noexcept
    {
        return (v / _scale) / _scale;
    }
    float from_squared(float v) const noexcept
    {
        return (v * _scale) * _scale;
    }
    elysia::core::Vector2 to_length(elysia::core::Vector2 v) const noexcept
    {
        return {to_length(v.x), to_length(v.y)};
    }
    elysia::core::Vector2 from_length(elysia::core::Vector2 v) const noexcept
    {
        return {from_length(v.x), from_length(v.y)};
    }

  private:
    float _scale;
};
} // namespace elysia::physics::detail
