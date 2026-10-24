#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"
#include "../layout/ui_list_layout.h"

namespace elysia::ui
{
enum class UiListDirection
{
    Vertical,
    Horizontal
};

class UiListContainer : public UiControlFocusScopeHost, private UiDelegatedFocusMixin
{
public:
    explicit UiListContainer(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiListContainer(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiListContainer(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiListContainer() override = default;

    void reset() noexcept override;
    void set_scope_focused(bool focused) noexcept override;
    bool focus_first_available() override;
    [[nodiscard]] bool can_navigate(UiAction action) const noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Prepends a child to the list while preserving container ownership.
    UiElement* add_front(std::unique_ptr<UiElement> child);
    // Appends a child to the list while preserving container ownership.
    UiElement* add_back(std::unique_ptr<UiElement> child);
    UiElement* add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options) override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;

    void set_direction(UiListDirection direction) noexcept;
    [[nodiscard]] UiListDirection direction() const noexcept;
    // Selects how children are positioned across the list's non-scrolling axis.
    void set_cross_align(UiLayoutAlign align) noexcept;
    [[nodiscard]] UiLayoutAlign cross_align() const noexcept;
    void set_item_spacing(float item_spacing) noexcept;
    [[nodiscard]] float item_spacing() const noexcept;

protected:
    // Lays out children along the configured list direction and spacing.
    void rebuild_layout() override;
    // Refreshes focus neighbors after list ordering or geometry changes.
    void rebuild_focus_registry() override;

private:
    [[nodiscard]] UiElement* neighbor_region_of(const UiControl* control,UiAction action) const noexcept;
    void sync_child_scope_focus() noexcept;
    [[nodiscard]] bool is_primary_axis_navigation(UiAction action) const noexcept;

private:
    layout::UiListLayoutConfig _layout{};
};
}
