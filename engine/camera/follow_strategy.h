#pragma once

#include "camera_focus.h"
#include "../core/geometry/vector2.h"

namespace elysia::camera
{
struct CameraFollowContext
{
    elysia::core::Vector2 current_center{};
    elysia::core::Vector2 viewport_size{};
    float zoom = 1.0f;
    bool automatic_zoom_enabled = true;
};

struct CameraFollowResult
{
    elysia::core::Vector2 center{};
    std::optional<float> zoom;
};

class IFollowStrategy
{
public:
    virtual ~IFollowStrategy() = default;
    virtual void reset() noexcept {}
    [[nodiscard]] virtual bool snap_on_acquisition() const noexcept { return true; }

    [[nodiscard]] virtual CameraFollowResult update(const CameraFollowContext& context,
        const CameraFocus& focus,double delta_seconds) = 0;
};

class HardFollowStrategy final : public IFollowStrategy
{
public:
    [[nodiscard]] CameraFollowResult update(const CameraFollowContext& context,
        const CameraFocus& focus,double delta_seconds) override;
};

class DeadZoneFollowStrategy final : public IFollowStrategy
{
public:
    explicit DeadZoneFollowStrategy(const elysia::core::Rect& dead_zone_rect) noexcept;

    void set_dead_zone_rect(const elysia::core::Rect& dead_zone_rect) noexcept;
    [[nodiscard]] const elysia::core::Rect& dead_zone_rect() const noexcept;

    [[nodiscard]] CameraFollowResult update(const CameraFollowContext& context,
        const CameraFocus& focus,double delta_seconds) override;

private:
    elysia::core::Rect _dead_zone_rect{};
};

class SmoothFollowStrategy final : public IFollowStrategy
{
public:
    explicit SmoothFollowStrategy(double follow_speed_units_per_second = 0.0) noexcept;

    void set_follow_speed_units_per_second(double follow_speed_units_per_second) noexcept;
    [[nodiscard]] double follow_speed_units_per_second() const noexcept;

    [[nodiscard]] CameraFollowResult update(const CameraFollowContext& context,
        const CameraFocus& focus,double delta_seconds) override;

private:
    double _follow_speed_units_per_second = 0.0;
};
}
