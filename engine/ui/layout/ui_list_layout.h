#pragma once

#include "ui_layout_types.h"
#include "ui_layout_geometry.h"
#include "../core/ui_child_host.h"

namespace elysia::ui::layout
{
// Shared list layout settings for vertical or horizontal child stacks.
struct UiListLayoutConfig
{
    UiLayoutDirection direction = UiLayoutDirection::Vertical;
    UiLayoutAlign cross_align = UiLayoutAlign::Center;
    float item_spacing = 16.0f;
};

// Places children sequentially along the configured list direction.
void layout_list_children(
    std::vector<UiChildHost::ChildEntry>& children,
    const elysia::core::Rect& bounds,
    const UiListLayoutConfig& config
) noexcept;

// Measures the padded extent required to lay out the current list children.
[[nodiscard]] elysia::core::Vector2 intrinsic_list_extent(
    const std::vector<UiChildHost::ChildEntry>& children,
    const UiLayoutPadding& padding,
    const UiListLayoutConfig& config
) noexcept;
}
