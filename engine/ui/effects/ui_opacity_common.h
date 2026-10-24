#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace elysia::ui::effects
{
// Clamps a normalized animation value into the inclusive [0, 1] range.
inline double clamp_unit(double t) noexcept
{
    return std::clamp(t,0.0,1.0);
}

// Converts elapsed time and duration into a clamped completion ratio.
inline double ratio(double value,double max_value) noexcept
{
    if (max_value <= 0.0)
        return 1.0;
    return clamp_unit(value / max_value);
}

// Applies a smooth ease-in/ease-out curve to an animation ratio.
inline double ease_in_out(double t) noexcept
{
    constexpr double kPi = 3.14159265358979323846;
    return 0.5 - 0.5 * std::cos(kPi * clamp_unit(t));
}

// Interpolates between two opacity values using a clamped normalized time.
inline std::uint8_t lerp_opacity(std::uint8_t from,std::uint8_t to,double t) noexcept
{
    const double alpha = static_cast<double>(from)
        + (static_cast<double>(to) - static_cast<double>(from)) * clamp_unit(t);
    return static_cast<std::uint8_t>(alpha);
}
}
