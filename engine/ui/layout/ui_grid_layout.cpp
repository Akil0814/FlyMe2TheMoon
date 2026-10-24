#include "ui_grid_layout.h"

#include <algorithm>

namespace elysia::ui::layout
{
void layout_grid_children(
    std::vector<UiChildHost::ChildEntry>& children,
    const elysia::core::Rect& bounds,
    const UiGridLayoutConfig& config
) noexcept
{
    if (children.empty())
        return;

    const int columns = std::max(1,config.column_count);
    const int count = static_cast<int>(children.size());
    const int rows = std::max(1,(count + columns - 1) / columns);
    const elysia::core::Vector2 spacing = clamp_size(config.cell_spacing);
    const float total_spacing_x = spacing.x * static_cast<float>(std::max(0,columns - 1));
    const float total_spacing_y = spacing.y * static_cast<float>(std::max(0,rows - 1));
    const float cell_width = std::max(0.0f,(bounds.width() - total_spacing_x) / static_cast<float>(columns));
    const float cell_height = std::max(0.0f,(bounds.height() - total_spacing_y) / static_cast<float>(rows));

    for (int index = 0; index < count; ++index)
    {
        UiChildHost::ChildEntry& child = children[static_cast<std::size_t>(index)];
        if (!child.element)
            continue;

        const int row = config.fill_by_row ? index / columns : index % rows;
        const int column = config.fill_by_row ? index % columns : index / rows;
        const elysia::core::Rect cell_rect(
            bounds.x() + static_cast<float>(column) * (cell_width + spacing.x),
            bounds.y() + static_cast<float>(row) * (cell_height + spacing.y),
            cell_width,
            cell_height
        );

        elysia::core::Vector2 child_size = child.layout._use_size_override
            ? clamp_size(child.layout._size_override)
            : clamp_size(child.element->size());
        if (child_size.x <= 0.0f || child_size.y <= 0.0f)
            child_size = cell_rect.size();

        child.element->set_screen_rect(aligned_rect_in_bounds(cell_rect,child_size,config.cell_anchor,child.layout._margin));
    }
}

elysia::core::Vector2 intrinsic_grid_extent(
    const std::vector<UiChildHost::ChildEntry>& children,
    const UiLayoutPadding& padding,
    const UiGridLayoutConfig& config
) noexcept
{
    int item_count = 0;
    float cell_width = 0.0f;
    float cell_height = 0.0f;

    for (const UiChildHost::ChildEntry& child : children)
    {
        if (!child.element)
            continue;

        const elysia::core::Vector2 extent = child.layout._use_size_override
            ? layout::clamp_size(child.layout._size_override)
            : layout::clamp_size(child.element->content_extent());
        cell_width = std::max(cell_width,extent.x);
        cell_height = std::max(cell_height,extent.y);
        ++item_count;
    }

    if (item_count == 0)
        return elysia::core::Vector2(padding.left + padding.right,padding.top + padding.bottom);

    const int columns = std::max(1,config.column_count);
    const int rows = std::max(1,(item_count + columns - 1) / columns);
    const elysia::core::Vector2 spacing = layout::clamp_size(config.cell_spacing);

    return elysia::core::Vector2(
        padding.left + static_cast<float>(columns) * cell_width + static_cast<float>(std::max(0,columns - 1)) * spacing.x + padding.right,
        padding.top + static_cast<float>(rows) * cell_height + static_cast<float>(std::max(0,rows - 1)) * spacing.y + padding.bottom
    );
}
}
