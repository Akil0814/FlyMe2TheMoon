#pragma once

#include "../focus/ui_control_focus_scope_host.h"
#include "../focus/ui_delegated_focus_mixin.h"
#include "../style/ui_style.h"
#include "../style/ui_visual_roles.h"
#include "../style/ui_visual_styles.h"

#include <vector>

namespace elysia::ui
{
enum class UiPanelInsertDirection
{
    Up,
    Down,
    Left,
    Right
};

class UiPanel : public UiControlFocusScopeHost, private UiDelegatedFocusMixin
{
public:
    explicit UiPanel(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiPanel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiPanel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiPanel() override = default;

    void reset() noexcept override;
    void set_scope_focused(bool focused) noexcept override;
    bool focus_first_available() override;
    [[nodiscard]] bool can_navigate(UiAction action) const noexcept override;
    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override;

    // Inserts a child relative to the last panel insertion point and updates focus links.
    UiElement* add_child(std::unique_ptr<UiElement> child,UiPanelInsertDirection direction = UiPanelInsertDirection::Down);
    UiElement* add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options) override;

    void set_base_style(const UiPanelStyle& style) noexcept;
    void set_style_overrides(const UiPanelStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiPanelStyle& style() const noexcept;
    [[nodiscard]] const UiPanelStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_visual_role(UiPanelVisualRole role) noexcept;
    [[nodiscard]] UiPanelVisualRole visual_role() const noexcept;

protected:
    // Repositions children using the panel's incremental insertion layout.
    void rebuild_layout() override;
    // Rebuilds directional focus neighbors based on insertion relationships.
    void rebuild_focus_registry() override;

private:
    // Stores directional navigation links for one focusable child in the panel flow.
    struct FocusLink
    {
        UiElement* element = nullptr;
        UiElement* up = nullptr;
        UiElement* down = nullptr;
        UiElement* left = nullptr;
        UiElement* right = nullptr;
    };

private:
    // Performs directional child insertion and returns the adopted child pointer.
    UiElement* insert_panel_child(std::unique_ptr<UiElement> child,UiPanelInsertDirection direction);
    void sync_child_scope_focus() noexcept;
    // Removes focus links that no longer reference live child controls.
    void prune_panel_links();
    // Returns or creates the focus-link record for a focusable direct child region.
    FocusLink& ensure_link(UiElement& element);
    FocusLink* find_link(UiElement& element) noexcept;
    const FocusLink* find_link(const UiElement& element) const noexcept;

private:
    UiStyleState<UiPanelStyle> _style_state;
    UiPanelVisualRole _visual_role = UiPanelVisualRole::Default;
    std::vector<FocusLink> _focus_links;
    UiElement* _last_focusable = nullptr;
    elysia::core::Vector2 _last_child_layout_origin{};
    bool _has_child_layout_origin = false;
};
}
