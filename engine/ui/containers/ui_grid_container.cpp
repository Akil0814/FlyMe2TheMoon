#include "ui_grid_container.h"

#include <algorithm>

namespace elysia::ui
{
namespace
{
[[nodiscard]] int row_of(int index,int columns,int rows,bool fill_by_row) noexcept
{
    return fill_by_row ? index / columns : index % rows;
}

[[nodiscard]] int column_of(int index,int columns,int rows,bool fill_by_row) noexcept
{
    return fill_by_row ? index % columns : index / rows;
}

[[nodiscard]] int index_of(int row,int column,int columns,int rows,bool fill_by_row) noexcept
{
    return fill_by_row ? row * columns + column : column * rows + row;
}
}

UiGridContainer::UiGridContainer(const elysia::core::Rect& rect,int order) noexcept
    : UiControlFocusScopeHost(rect,order) {}

UiGridContainer::UiGridContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiControlFocusScopeHost(position,size,order) {}

UiGridContainer::UiGridContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiControlFocusScopeHost(center,size,from_center,order) {}

void UiGridContainer::reset() noexcept
{
    UiControlFocusScopeHost::reset();
    _layout = layout::UiGridLayoutConfig{};
}

UiElement* UiGridContainer::add_child(std::unique_ptr<UiElement> child)
{
    return insert_child(std::move(child),child_count(),UiLayoutChildOptions{});
}

UiElement* UiGridContainer::add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    return insert_child(std::move(child),child_count(),options);
}

elysia::core::Vector2 UiGridContainer::content_extent() const noexcept
{
    const elysia::core::Vector2 intrinsic = layout::intrinsic_grid_extent(children(),padding(),_layout);
    const elysia::core::Vector2 explicit_size = size();
    return elysia::core::Vector2(
        std::max(explicit_size.x,intrinsic.x),
        std::max(explicit_size.y,intrinsic.y)
    );
}

void UiGridContainer::set_column_count(int column_count) noexcept
{
    _layout.column_count = std::max(1,column_count);
    invalidate_intrinsic_layout();
}

int UiGridContainer::column_count() const noexcept
{
    return _layout.column_count;
}

void UiGridContainer::set_cell_spacing(const elysia::core::Vector2& spacing) noexcept
{
    _layout.cell_spacing = layout::clamp_size(spacing);
    invalidate_intrinsic_layout();
}

elysia::core::Vector2 UiGridContainer::cell_spacing() const noexcept
{
    return _layout.cell_spacing;
}

void UiGridContainer::set_fill_by_row(bool fill_by_row) noexcept
{
    _layout.fill_by_row = fill_by_row;
    invalidate_intrinsic_layout();
}

bool UiGridContainer::fills_by_row() const noexcept
{
    return _layout.fill_by_row;
}

void UiGridContainer::rebuild_layout()
{
    layout::layout_grid_children(children(),content_rect(),_layout);
}

void UiGridContainer::rebuild_focus_registry()
{
    const int total_children = static_cast<int>(child_count());
    const int columns = std::max(1,_layout.column_count);
    const int rows = std::max(1,(total_children + columns - 1) / columns);

    std::vector<UiControl*> indexed_controls(static_cast<std::size_t>(total_children),nullptr);
    for (int index = 0; index < total_children; ++index)
    {
        if (UiElement* element = child_at(static_cast<std::size_t>(index)))
            indexed_controls[static_cast<std::size_t>(index)] = dynamic_cast<UiControl*>(element);
    }

    auto find_neighbor = [&](int start_row,int start_column,int row_step,int column_step) -> UiControl*
    {
        int row = start_row + row_step;
        int column = start_column + column_step;
        while (row >= 0 && row < rows && column >= 0 && column < columns)
        {
            const int target = index_of(row,column,columns,rows,_layout.fill_by_row);
            if (target >= 0 && target < total_children)
            {
                if (UiControl* candidate = indexed_controls[static_cast<std::size_t>(target)])
                    return candidate;
            }
            row += row_step;
            column += column_step;
        }
        return nullptr;
    };

    std::vector<FocusEntry> entries;
    for (int index = 0; index < total_children; ++index)
    {
        UiControl* control = indexed_controls[static_cast<std::size_t>(index)];
        if (!control)
            continue;

        const int row = row_of(index,columns,rows,_layout.fill_by_row);
        const int column = column_of(index,columns,rows,_layout.fill_by_row);
        UiFocusNeighbors neighbors;
        if (UiControl* left = find_neighbor(row,column,0,-1))
            neighbors.left = left;
        if (UiControl* right = find_neighbor(row,column,0,1))
            neighbors.right = right;
        if (UiControl* up = find_neighbor(row,column,-1,0))
            neighbors.up = up;
        if (UiControl* down = find_neighbor(row,column,1,0))
            neighbors.down = down;
        entries.push_back(FocusEntry{ control,neighbors });
    }

    set_focus_entries(std::move(entries));
}
}
