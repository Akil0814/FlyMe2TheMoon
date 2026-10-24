#pragma once

#include "ui_layout_geometry.h"
#include "../core/ui_child_host.h"

namespace elysia::ui::layout
{
// Shared grid layout settings for containers that place children in uniform cells.
struct UiGridLayoutConfig
{
    int column_count = 4;
    elysia::core::Vector2 cell_spacing{ 12.0f,12.0f };
    UiLayoutAnchor cell_anchor = UiLayoutAnchor::Center;
    bool fill_by_row = true;
};

// Places children into grid cells within the supplied bounds.
void layout_grid_children(
    std::vector<UiChildHost::ChildEntry>& children,
    const elysia::core::Rect& bounds,
    const UiGridLayoutConfig& config
) noexcept;

// Measures the padded extent required to lay out the current grid children.
[[nodiscard]] elysia::core::Vector2 intrinsic_grid_extent(
    const std::vector<UiChildHost::ChildEntry>& children,
    const UiLayoutPadding& padding,
    const UiGridLayoutConfig& config
) noexcept;
}
