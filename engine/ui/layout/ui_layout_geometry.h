#pragma once

#include "ui_layout_types.h"
#include "../../core/geometry/rect.h"

namespace elysia::ui::layout
{
// Prevents negative layout dimensions from propagating into geometry math.
[[nodiscard]] float clamp_non_negative(float value) noexcept;
// Clamps width and height independently so layout sizes stay non-negative.
[[nodiscard]] elysia::core::Vector2 clamp_size(const elysia::core::Vector2& size) noexcept;
// Returns the child layout region after subtracting host padding from a rect.
[[nodiscard]] elysia::core::Rect padded_content_rect(const elysia::core::Rect& rect,const UiLayoutPadding& padding) noexcept;
// Places a rect inside bounds using the requested anchor and child margin.
[[nodiscard]] elysia::core::Rect anchored_rect(
    const elysia::core::Rect& bounds,
    const elysia::core::Vector2& size,
    UiLayoutAnchor anchor,
    const UiLayoutMargin& margin
) noexcept;
// Aligns a rect within bounds while honoring anchor semantics shared across layouts.
[[nodiscard]] elysia::core::Rect aligned_rect_in_bounds(
    const elysia::core::Rect& bounds,
    const elysia::core::Vector2& size,
    UiLayoutAnchor anchor,
    const UiLayoutMargin& margin
) noexcept;
}
