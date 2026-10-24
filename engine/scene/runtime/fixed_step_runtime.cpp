#include "fixed_step_runtime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace elysia::scene
{
FixedStepRuntime::FixedStepRuntime(FixedStepConfig config) : _config(config)
{
    if (!std::isfinite(config.delta_seconds) || config.delta_seconds <= 0.0 ||
        config.max_steps_per_frame == 0)
    {
        throw std::invalid_argument("Invalid fixed-step configuration.");
    }
}

void FixedStepRuntime::advance(
    double delta_seconds,
    const std::function<void(std::uint64_t, double)>& step)
{
    _stats.executed_steps = 0;
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0)
    {
        _stats.interpolation_alpha =
            std::clamp(_accumulator_seconds / _config.delta_seconds, 0.0, 1.0);
        return;
    }

    _accumulator_seconds += delta_seconds;
    while (_accumulator_seconds + std::numeric_limits<double>::epsilon() >=
               _config.delta_seconds &&
           _stats.executed_steps < _config.max_steps_per_frame)
    {
        _accumulator_seconds -= _config.delta_seconds;
        ++_stats.executed_steps;
        step(++_tick, _config.delta_seconds);
    }

    if (_accumulator_seconds >= _config.delta_seconds)
    {
        const double dropped = std::floor(_accumulator_seconds / _config.delta_seconds);
        const auto room = std::numeric_limits<std::uint64_t>::max() - _stats.dropped_steps;
        _stats.dropped_steps += dropped >= static_cast<double>(room)
            ? room
            : static_cast<std::uint64_t>(dropped);
        _accumulator_seconds = std::fmod(_accumulator_seconds, _config.delta_seconds);
    }

    _stats.interpolation_alpha =
        std::clamp(_accumulator_seconds / _config.delta_seconds, 0.0, 1.0);
}

void FixedStepRuntime::reset() noexcept
{
    _stats = {};
    _accumulator_seconds = 0.0;
    _tick = 0;
}
} // namespace elysia::scene
