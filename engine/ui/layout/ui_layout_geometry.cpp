#include "ui_layout_geometry.h"

#include <algorithm>

namespace elysia::ui::layout
{
namespace
{
[[nodiscard]] float horizontal_margin_offset(UiLayoutAnchor anchor,const UiLayoutMargin& margin) noexcept
{
    switch (anchor)
    {
    case UiLayoutAnchor::TopLeft:
    case UiLayoutAnchor::CenterLeft:
    case UiLayoutAnchor::BottomLeft:
        return margin.left;
    case UiLayoutAnchor::TopCenter:
    case UiLayoutAnchor::Center:
    case UiLayoutAnchor::BottomCenter:
        return margin.left - margin.right;
    case UiLayoutAnchor::TopRight:
    case UiLayoutAnchor::CenterRight:
    case UiLayoutAnchor::BottomRight:
        return -margin.right;
    default:
        return 0.0f;
    }
}

[[nodiscard]] float vertical_margin_offset(UiLayoutAnchor anchor,const UiLayoutMargin& margin) noexcept
{
    switch (anchor)
    {
    case UiLayoutAnchor::TopLeft:
    case UiLayoutAnchor::TopCenter:
    case UiLayoutAnchor::TopRight:
        return margin.top;
    case UiLayoutAnchor::CenterLeft:
    case UiLayoutAnchor::Center:
    case UiLayoutAnchor::CenterRight:
        return margin.top - margin.bottom;
    case UiLayoutAnchor::BottomLeft:
    case UiLayoutAnchor::BottomCenter:
    case UiLayoutAnchor::BottomRight:
        return -margin.bottom;
    default:
        return 0.0f;
    }
}
}

float clamp_non_negative(float value) noexcept
{
    return std::max(0.0f,value);
}

elysia::core::Vector2 clamp_size(const elysia::core::Vector2& size) noexcept
{
    return elysia::core::Vector2(clamp_non_negative(size.x),clamp_non_negative(size.y));
}

elysia::core::Rect padded_content_rect(const elysia::core::Rect& rect,const UiLayoutPadding& padding) noexcept
{
    const float width = std::max(0.0f,rect.width() - padding.left - padding.right);
    const float height = std::max(0.0f,rect.height() - padding.top - padding.bottom);
    return elysia::core::Rect(rect.x() + padding.left,rect.y() + padding.top,width,height);
}

elysia::core::Rect anchored_rect(
    const elysia::core::Rect& bounds,
    const elysia::core::Vector2& size,
    UiLayoutAnchor anchor,
    const UiLayoutMargin& margin
) noexcept
{
    const elysia::core::Vector2 clamped_size = clamp_size(size);
    elysia::core::Rect rect = elysia::core::Rect::from_center(bounds.center(),clamped_size);

    switch (anchor)
    {
    case UiLayoutAnchor::TopLeft:
        rect.set_position(bounds.top_left());
        break;
    case UiLayoutAnchor::TopCenter:
        rect.set_position(elysia::core::Vector2(bounds.center().x - clamped_size.x * 0.5f,bounds.top()));
        break;
    case UiLayoutAnchor::TopRight:
        rect.set_position(elysia::core::Vector2(bounds.right() - clamped_size.x,bounds.top()));
        break;
    case UiLayoutAnchor::CenterLeft:
        rect.set_position(elysia::core::Vector2(bounds.left(),bounds.center().y - clamped_size.y * 0.5f));
        break;
    case UiLayoutAnchor::Center:
        rect.set_center(bounds.center());
        break;
    case UiLayoutAnchor::CenterRight:
        rect.set_position(elysia::core::Vector2(bounds.right() - clamped_size.x,bounds.center().y - clamped_size.y * 0.5f));
        break;
    case UiLayoutAnchor::BottomLeft:
        rect.set_position(elysia::core::Vector2(bounds.left(),bounds.bottom() - clamped_size.y));
        break;
    case UiLayoutAnchor::BottomCenter:
        rect.set_position(elysia::core::Vector2(bounds.center().x - clamped_size.x * 0.5f,bounds.bottom() - clamped_size.y));
        break;
    case UiLayoutAnchor::BottomRight:
        rect.set_position(elysia::core::Vector2(bounds.right() - clamped_size.x,bounds.bottom() - clamped_size.y));
        break;
    }

    rect.set_position(elysia::core::Vector2(
        rect.x() + horizontal_margin_offset(anchor,margin),
        rect.y() + vertical_margin_offset(anchor,margin)
    ));
    return rect;
}

elysia::core::Rect aligned_rect_in_bounds(
    const elysia::core::Rect& bounds,
    const elysia::core::Vector2& size,
    UiLayoutAnchor anchor,
    const UiLayoutMargin& margin
) noexcept
{
    const elysia::core::Vector2 clamped_size(
        std::min(clamp_non_negative(size.x),bounds.width()),
        std::min(clamp_non_negative(size.y),bounds.height())
    );
    return anchored_rect(bounds,clamped_size,anchor,margin);
}
}
