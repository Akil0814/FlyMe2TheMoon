#pragma once

#include "../core/geometry/rect.h"
#include <optional>
#include <span>

namespace elysia::camera
{
struct CameraFocus
{
    elysia::core::Rect bounds{};
    elysia::core::Rect primary{};
};

[[nodiscard]] inline bool valid_focus_rect(const elysia::core::Rect& rect) noexcept
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
        && std::isfinite(rect.right()) && std::isfinite(rect.bottom())
        && std::isfinite(rect.width()) && std::isfinite(rect.height());
}

// Values are copied; the source rectangles need not outlive this call.
[[nodiscard]] inline std::optional<CameraFocus> make_camera_focus(
    std::span<const elysia::core::Rect> rects, std::size_t primary_index = 0) noexcept
{
    std::optional<CameraFocus> focus;
    for (const auto& rect : rects)
    {
        if (!valid_focus_rect(rect)) continue;
        if (!focus) focus = CameraFocus{rect, rect};
        else
        {
            const auto& bounds = focus->bounds;
            focus->bounds = elysia::core::Rect::from_points(
                {std::min(bounds.left(), rect.left()), std::min(bounds.top(), rect.top())},
                {std::max(bounds.right(), rect.right()), std::max(bounds.bottom(), rect.bottom())});
        }
    }
    if (focus && primary_index < rects.size() && valid_focus_rect(rects[primary_index]))
        focus->primary = rects[primary_index];
    if (focus && !valid_focus_rect(focus->bounds)) return std::nullopt;
    return focus;
}
}
