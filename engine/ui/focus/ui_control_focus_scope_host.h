#pragma once

#include "ui_focus_scope.h"
#include "ui_focus_scope_utils.h"
#include "../core/ui_child_host.h"
#include "../core/ui_control.h"
#include "../../input/input_types.h"

#include <vector>

namespace elysia::ui
{
class UiDelegatedFocusMixin;

class UiControlFocusScopeHost : public UiChildHost, public UiFocusScope
{
    friend class UiDelegatedFocusMixin;

public:
    // Binds a focusable child to its directional neighbors within this scope.
    struct FocusEntry
    {
        UiControl* control = nullptr;
        UiFocusNeighbors neighbors;
    };

public:
    explicit UiControlFocusScopeHost(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0) noexcept;
    UiControlFocusScopeHost(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order = 0) noexcept;
    UiControlFocusScopeHost(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order = 0) noexcept;
    ~UiControlFocusScopeHost() override = default;

    void reset() noexcept override;

    // Moves focus to a registered child control when the target is still valid.
    void set_focused_target(UiControl* control);
    [[nodiscard]] UiControl* focused_target() const noexcept override;
    bool focus_first_available() override;
    [[nodiscard]] bool has_focusable_target() const noexcept override;
    [[nodiscard]] bool can_navigate(UiAction action) const noexcept override;
    bool clear_focus_for_gamepad_scroll() override;
    bool restore_focus_after_gamepad_scroll() override;
    [[nodiscard]] elysia::input::InputDevice focus_input_device() const noexcept { return _focus_input_device; }

    [[nodiscard]] UiElement& focus_scope_element() noexcept override;
    [[nodiscard]] const UiElement& focus_scope_element() const noexcept override;
    void set_scope_focused(bool focused) noexcept override;
    [[nodiscard]] bool is_scope_focused() const noexcept override;
    [[nodiscard]] bool contains_focus_point(int mouse_x,int mouse_y) const noexcept override;

    void update(double delta) override;
    void on_ui_input_frame(const UiInputFrame& input) override;
    bool on_ui_input_event(const UiInputEvent& event) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

protected:
    // Repairs child ownership, layout, focus registry, and focused visuals at
    // explicit lifecycle boundaries. Query methods never invoke this implicitly.
    void synchronize_focus_state();
    virtual void rebuild_focus_registry() = 0;

    // Rebuilds directional focus links after children or layout relationships change.
    void refresh_focus_registry();
    // Replaces the current focus graph with caller-provided directional entries.
    void set_focus_entries(std::vector<FocusEntry> entries);
    [[nodiscard]] const std::vector<FocusEntry>& focus_entries() const noexcept;
    [[nodiscard]] std::vector<FocusEntry>& focus_entries() noexcept;
    // Returns direct child controls that participate in this scope's focus graph.
    [[nodiscard]] std::vector<UiControl*> direct_focusable_children() const;
    // Repairs stale focus after children are removed, disabled, or hidden.
    void ensure_valid_focus();
    // Pushes the scope's current focus state down into registered child controls.
    void apply_focus_state() const;
    // Finds the registered focus target under a pointer position for hover focus.
    [[nodiscard]] UiControl* find_registered_target_at(int mouse_x,int mouse_y) const;
    [[nodiscard]] bool is_registered_focus_target(const UiControl* control) const noexcept;
    // Performs focus assignment without re-entering public validation paths.
    [[nodiscard]] bool set_focused_target_internal(UiControl* control) noexcept;
    // Tracks the device that most recently drove focus decisions.
    void update_focus_input_device(elysia::input::InputDevice device) noexcept;
    // Restores the previously focused child when it is still usable.
    bool restore_preferred_focus_target();
    [[nodiscard]] static bool uses_pointer_focus_policy(elysia::input::InputDevice device) noexcept;

private:
    [[nodiscard]] UiControl* find_neighbor(const UiControl& control,UiAction action) const noexcept;

private:
    std::vector<FocusEntry> _focus_entries;
    UiControl* _focused_target = nullptr;
    UiControl* _last_focused_target = nullptr;
    bool _scope_focused = false;
    bool _gamepad_scroll_focus_suppressed = false;
    elysia::input::InputDevice _focus_input_device = elysia::input::InputDevice::Unknown;
};
}
