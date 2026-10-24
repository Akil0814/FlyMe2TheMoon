#include "camera.h"

#include <algorithm>
#include <cmath>

namespace elysia::camera
{
namespace
{
[[nodiscard]] elysia::core::Vector2 clamp_non_negative(
    const elysia::core::Vector2& value
) noexcept
{
    return elysia::core::Vector2(
        std::max(0.0f, value.x),
        std::max(0.0f, value.y)
    );
}
}

Camera::Camera(
    const elysia::core::Vector2& center,
    const elysia::core::Vector2& viewport_size,
    float zoom
) noexcept
    : _center(center),
      _viewport_size(clamp_non_negative(viewport_size)),
      _zoom(clamp_zoom(zoom))
{
}

void Camera::set_center(const elysia::core::Vector2& center) noexcept
{
    _center = center;
}

void Camera::set_viewport_size(const elysia::core::Vector2& viewport_size) noexcept
{
    _viewport_size = clamp_non_negative(viewport_size);
}

void Camera::set_zoom(float zoom) noexcept
{
    _zoom = clamp_zoom(zoom);
}

const elysia::core::Vector2& Camera::center() const noexcept
{
    return _center;
}

const elysia::core::Vector2& Camera::viewport_size() const noexcept
{
    return _viewport_size;
}

float Camera::zoom() const noexcept
{
    return _zoom;
}

elysia::core::Vector2 Camera::world_viewport_size() const noexcept
{
    return _viewport_size / _zoom;
}

float Camera::clamp_zoom(float zoom) noexcept
{
    if (!std::isfinite(zoom))
        return kDefaultZoom;

    return std::clamp(zoom, kMinZoom, kMaxZoom);
}

elysia::core::Rect Camera::view_rect() const noexcept
{
    return elysia::core::Rect::from_center(_center, world_viewport_size());
}

elysia::core::Vector2 Camera::world_to_screen(
    const elysia::core::Vector2& world_position
) const noexcept
{
    const elysia::core::Rect current_view = view_rect();
    return (world_position - current_view.top_left()) * _zoom;
}

elysia::core::Rect Camera::world_to_screen(
    const elysia::core::Rect& world_rect
) const noexcept
{
    return elysia::core::Rect(
        world_to_screen(world_rect.position()),
        world_rect.size() * _zoom
    );
}

elysia::core::Vector2 Camera::screen_to_world(
    const elysia::core::Vector2& screen_position
) const noexcept
{
    return view_rect().top_left() + screen_position / _zoom;
}

elysia::core::Rect Camera::screen_to_world(
    const elysia::core::Rect& screen_rect
) const noexcept
{
    return elysia::core::Rect(
        screen_to_world(screen_rect.position()),
        screen_rect.size() / _zoom
    );
}
}
