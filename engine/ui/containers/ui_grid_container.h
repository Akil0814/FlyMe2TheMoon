#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../layout/ui_grid_layout.h"

namespace elysia::ui
{
class UiGridContainer : public UiControlFocusScopeHost
{
public:
    explicit UiGridContainer(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiGridContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiGridContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiGridContainer() override = default;

    void reset() noexcept override;

    // Adds a child using the container's current grid layout defaults.
    UiElement* add_child(std::unique_ptr<UiElement> child);
    UiElement* add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options) override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;

    void set_column_count(int column_count) noexcept;
    [[nodiscard]] int column_count() const noexcept;
    void set_cell_spacing(const elysia::core::Vector2& spacing) noexcept;
    [[nodiscard]] elysia::core::Vector2 cell_spacing() const noexcept;
    void set_fill_by_row(bool fill_by_row) noexcept;
    [[nodiscard]] bool fills_by_row() const noexcept;

protected:
    // Lays out children into grid cells within the container content rect.
    void rebuild_layout() override;
    // Refreshes focus neighbors after the grid changes child ordering or geometry.
    void rebuild_focus_registry() override;

private:
    layout::UiGridLayoutConfig _layout{};
};
}
