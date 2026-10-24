#pragma once
#include "follow_strategy.h"

namespace elysia::camera
{
struct MultiTargetFollowConfig
{
    bool dead_zone_enabled = true;
    float safe_ratio = 0.70f;
    float inner_ratio = 0.55f;
    float min_zoom = 0.5f;
    float max_zoom = 2.0f;
    double movement_half_life = 0.12;
    double zoom_out_half_life = 0.10;
    double zoom_in_half_life = 0.35;
    double settle_seconds = 0.4;
};

class MultiTargetFollowStrategy final : public IFollowStrategy
{
public:
    explicit MultiTargetFollowStrategy(MultiTargetFollowConfig config = {}) noexcept;
    void set_config(MultiTargetFollowConfig config) noexcept;
    [[nodiscard]] const MultiTargetFollowConfig& config() const noexcept { return _config; }
    [[nodiscard]] bool primary_only() const noexcept { return _primary_only; }
    [[nodiscard]] bool snap_on_acquisition() const noexcept override { return false; }
    void reset() noexcept override;
    [[nodiscard]] CameraFollowResult update(const CameraFollowContext& context,
        const CameraFocus& focus, double delta_seconds) override;

private:
    MultiTargetFollowConfig _config;
    double _zoom_in_time = 0.0;
    double _recovery_time = 0.0;
    bool _zooming_in = false;
    bool _primary_only = false;
};
}
