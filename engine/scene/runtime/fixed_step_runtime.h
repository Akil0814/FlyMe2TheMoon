#pragma once

#include "scene_runtime_features.h"

#include <cstdint>
#include <functional>

namespace elysia::scene
{
struct FixedStepStats
{
    std::uint32_t executed_steps = 0;
    std::uint64_t dropped_steps = 0;
    double interpolation_alpha = 0.0;
};

class FixedStepRuntime final
{
public:
    explicit FixedStepRuntime(FixedStepConfig config);

    void advance(double delta_seconds, const std::function<void(std::uint64_t, double)>& step);
    void reset() noexcept;

    [[nodiscard]] const FixedStepConfig& config() const noexcept { return _config; }
    [[nodiscard]] const FixedStepStats& stats() const noexcept { return _stats; }
    [[nodiscard]] std::uint64_t tick() const noexcept { return _tick; }

private:
    FixedStepConfig _config;
    FixedStepStats _stats;
    double _accumulator_seconds = 0.0;
    std::uint64_t _tick = 0;
};
} // namespace elysia::scene
