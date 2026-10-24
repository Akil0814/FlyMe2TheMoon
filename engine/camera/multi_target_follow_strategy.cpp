#include "multi_target_follow_strategy.h"
#include "camera.h"
#include <cmath>
#include <limits>

namespace elysia::camera
{
namespace
{
float fit_zoom(const elysia::core::Rect& bounds, elysia::core::Vector2 viewport, float ratio)
{
    const float infinity = std::numeric_limits<float>::infinity();

    return std::min(bounds.width() > 0 ? viewport.x * ratio / bounds.width() : infinity,
        bounds.height() > 0 ? viewport.y * ratio / bounds.height() : infinity);
}

float blend(double dt, double half_life)
{
    return static_cast<float>(-std::expm1(-std::log(2.0) * dt / half_life));
}

float center_axis(float current, float low, float high, float visible)
{
    if (high - low > visible)
        return low + (high - low) * 0.5f;

    return std::clamp(current, high - visible * 0.5f, low + visible * 0.5f);
}

bool inside(const elysia::core::Rect& bounds, elysia::core::Vector2 center,
    elysia::core::Vector2 size)
{
    const auto view = elysia::core::Rect::from_center(center, size);

    return bounds.left() >= view.left() && bounds.right() <= view.right()
        && bounds.top() >= view.top() && bounds.bottom() <= view.bottom();
}
}

MultiTargetFollowStrategy::MultiTargetFollowStrategy(MultiTargetFollowConfig config) noexcept
{
    set_config(config);
}

void MultiTargetFollowStrategy::set_config(MultiTargetFollowConfig config) noexcept
{
    auto positive = [](double value, double fallback) {
        return std::isfinite(value) && value > 0 ? value : fallback;
    };

    config.safe_ratio = std::clamp(static_cast<float>(positive(config.safe_ratio, 0.70)), 0.02f, 1.0f);
    config.inner_ratio = std::clamp(static_cast<float>(positive(config.inner_ratio, 0.55)),
        0.01f, config.safe_ratio - 0.01f);
    config.min_zoom = Camera::clamp_zoom(config.min_zoom);
    config.max_zoom = std::max(config.min_zoom, Camera::clamp_zoom(config.max_zoom));
    config.movement_half_life = positive(config.movement_half_life, 0.12);
    config.zoom_out_half_life = positive(config.zoom_out_half_life, 0.10);
    config.zoom_in_half_life = positive(config.zoom_in_half_life, 0.35);
    config.settle_seconds = positive(config.settle_seconds, 0.4);

    _config = config;

    reset();
}

void MultiTargetFollowStrategy::reset() noexcept
{
    _zoom_in_time = _recovery_time = 0.0;

    _zooming_in = _primary_only = false;
}

CameraFollowResult MultiTargetFollowStrategy::update(const CameraFollowContext& context,
    const CameraFocus& focus, double dt)
{
    if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(context.viewport_size.x)
        || !std::isfinite(context.viewport_size.y) || context.viewport_size.x <= 0
        || context.viewport_size.y <= 0 || !valid_focus_rect(focus.bounds)
        || !valid_focus_rect(focus.primary))
        return {context.current_center, std::nullopt};

    const auto& c = _config;
    const float fit = fit_zoom(focus.bounds, context.viewport_size, c.safe_ratio);
    if (fit < c.min_zoom)
    {
        _primary_only = true;
        _recovery_time = 0;
    }
    else if (_primary_only)
    {
        // Recovery concerns whether the group fits, independent of its current screen position.
        if (fit_zoom(focus.bounds, context.viewport_size, c.inner_ratio) >= c.min_zoom)
            _recovery_time += dt;
        else _recovery_time = 0;

        if (_recovery_time >= c.settle_seconds)
            _primary_only = false;
    }

    float zoom = Camera::clamp_zoom(context.zoom);
    std::optional<float> output_zoom;
    if (context.automatic_zoom_enabled)
    {
        const float target = _primary_only ? c.min_zoom : std::clamp(fit, c.min_zoom, c.max_zoom);
        double zoom_dt = dt;
        if (target < zoom || _primary_only)
        {
            _zoom_in_time = 0;
            _zooming_in = false;
        }
        else if (!_zooming_in)
        {
            if (inside(focus.bounds, context.current_center, context.viewport_size * (c.inner_ratio / zoom)))
            {
                const double previous = _zoom_in_time;
                _zoom_in_time += dt;
                _zooming_in = _zoom_in_time >= c.settle_seconds;
                zoom_dt = std::max(0.0, dt - std::max(0.0, c.settle_seconds - previous));
            }
            else
                _zoom_in_time = 0;
        }
        // Bring an explicit manual zoom outside the configured range back smoothly as well.
        if (target < zoom || _zooming_in || _primary_only || zoom < c.min_zoom)
            zoom += (target - zoom) * blend(zoom_dt,
                target < zoom ? c.zoom_out_half_life : c.zoom_in_half_life);

        output_zoom = zoom;
    }
    else
    {
        _zoom_in_time = 0;
        _zooming_in = false;
    }

    const auto& bounds = _primary_only ? focus.primary : focus.bounds;
    auto target_center = bounds.center();
    if (c.dead_zone_enabled)
    {
        const auto size = context.viewport_size * (c.safe_ratio / zoom);
        target_center = {
            center_axis(context.current_center.x, bounds.left(), bounds.right(), size.x),
            center_axis(context.current_center.y, bounds.top(), bounds.bottom(), size.y)};
    }
    return {context.current_center + (target_center - context.current_center) * blend(dt, c.movement_half_life),
        output_zoom};
}
}
