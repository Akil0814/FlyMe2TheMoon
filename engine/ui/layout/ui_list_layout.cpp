#include "ui_list_layout.h"

#include <algorithm>

namespace elysia::ui::layout
{
namespace
{
[[nodiscard]] UiLayoutAnchor list_anchor(UiLayoutDirection direction,UiLayoutAlign align) noexcept
{
    if (direction == UiLayoutDirection::Vertical)
    {
        switch (align)
        {
        case UiLayoutAlign::Start:
            return UiLayoutAnchor::TopLeft;
        case UiLayoutAlign::End:
            return UiLayoutAnchor::TopRight;
        case UiLayoutAlign::Center:
        default:
            return UiLayoutAnchor::TopCenter;
        }
    }

    switch (align)
    {
    case UiLayoutAlign::Start:
        return UiLayoutAnchor::TopLeft;
    case UiLayoutAlign::End:
        return UiLayoutAnchor::BottomLeft;
    case UiLayoutAlign::Center:
    default:
        return UiLayoutAnchor::CenterLeft;
    }
}
}

void layout_list_children(
    std::vector<UiChildHost::ChildEntry>& children,
    const elysia::core::Rect& bounds,
    const UiListLayoutConfig& config
) noexcept
{
    float cursor = 0.0f;
    const float item_spacing = clamp_non_negative(config.item_spacing);

    for (UiChildHost::ChildEntry& child : children)
    {
        if (!child.element)
            continue;

        // content_extent() is the child contract for desired/minimum layout size.
        // size() only describes geometry allocated by a previous layout pass.
        elysia::core::Vector2 child_size = child.layout._use_size_override
            ? clamp_size(child.layout._size_override)
            : clamp_size(child.element->content_extent());
        const UiLayoutAlign cross_align = child.layout._use_custom_cross_align ? child.layout._cross_align : config.cross_align;
        const UiLayoutAnchor anchor = list_anchor(config.direction,cross_align);

        if (config.direction == UiLayoutDirection::Vertical)
        {
            if (child.layout._fill_cross_axis)
                child_size.x = bounds.width();

            // Constrain the cross axis before the final desired-height read. Widgets such
            // as UiTextBlock measure wrapping from their current width, so a width change
            // must be observable before their main-axis extent is consumed.
            if (!child.layout._use_size_override)
            {
                child_size.x = std::min(child_size.x,bounds.width());
                elysia::core::Rect measure_rect = child.element->screen_rect();
                measure_rect.set_size(child_size);
                child.element->set_screen_rect(measure_rect);
                child_size.y = clamp_size(child.element->content_extent()).y;
            }
            const elysia::core::Rect slot_rect(bounds.x(),bounds.y() + cursor,bounds.width(),child_size.y);
            child.element->set_screen_rect(aligned_rect_in_bounds(slot_rect,child_size,anchor,child.layout._margin));
            cursor += child_size.y + item_spacing;
        }
        else
        {
            if (child.layout._fill_cross_axis)
                child_size.y = bounds.height();

            if (!child.layout._use_size_override)
            {
                child_size.y = std::min(child_size.y,bounds.height());
                elysia::core::Rect measure_rect = child.element->screen_rect();
                measure_rect.set_size(child_size);
                child.element->set_screen_rect(measure_rect);
                child_size.x = clamp_size(child.element->content_extent()).x;
            }
            const elysia::core::Rect slot_rect(bounds.x() + cursor,bounds.y(),child_size.x,bounds.height());
            child.element->set_screen_rect(aligned_rect_in_bounds(slot_rect,child_size,anchor,child.layout._margin));
            cursor += child_size.x + item_spacing;
        }
    }
}

elysia::core::Vector2 intrinsic_list_extent(
    const std::vector<UiChildHost::ChildEntry>& children,
    const UiLayoutPadding& padding,
    const UiListLayoutConfig& config
) noexcept
{
    const float item_spacing = clamp_non_negative(config.item_spacing);
    float main_axis_extent = 0.0f;
    float cross_axis_extent = 0.0f;
    bool has_item = false;

    for (const UiChildHost::ChildEntry& child : children)
    {
        if (!child.element)
            continue;

        const elysia::core::Vector2 extent = child.layout._use_size_override
            ? clamp_size(child.layout._size_override)
            : clamp_size(child.element->content_extent());

        if (has_item)
            main_axis_extent += item_spacing;

        if (config.direction == UiLayoutDirection::Vertical)
        {
            main_axis_extent += extent.y;
            cross_axis_extent = std::max(cross_axis_extent,extent.x);
        }
        else
        {
            main_axis_extent += extent.x;
            cross_axis_extent = std::max(cross_axis_extent,extent.y);
        }

        has_item = true;
    }

    if (config.direction == UiLayoutDirection::Vertical)
    {
        return elysia::core::Vector2(
            padding.left + cross_axis_extent + padding.right,
            padding.top + main_axis_extent + padding.bottom
        );
    }

    return elysia::core::Vector2(
        padding.left + main_axis_extent + padding.right,
        padding.top + cross_axis_extent + padding.bottom
    );
}
}
