#pragma once

#include "../input/ui_input_types.h"

#include <optional>

namespace elysia::ui
{
class UiControl;
class UiElement;
class UiFocusScope;

// Directional links used for focus navigation between controls inside one scope.
struct UiFocusNeighbors
{
    std::optional<UiControl*> up;
    std::optional<UiControl*> down;
    std::optional<UiControl*> left;
    std::optional<UiControl*> right;
};

// Directional links used for moving focus between sibling focus scopes.
struct UiFocusScopeNeighbors
{
    UiFocusScope* up = nullptr;
    UiFocusScope* down = nullptr;
    UiFocusScope* left = nullptr;
    UiFocusScope* right = nullptr;
};

class UiFocusScope
{
public:
    virtual ~UiFocusScope() = default;

    // Exposes the element whose bounds and lifecycle define this focus scope.
    [[nodiscard]] virtual UiElement& focus_scope_element() noexcept = 0;
    [[nodiscard]] virtual const UiElement& focus_scope_element() const noexcept = 0;
    // Enables or suppresses focus visuals and navigation for the entire scope.
    virtual void set_scope_focused(bool focused) noexcept = 0;
    [[nodiscard]] virtual bool is_scope_focused() const noexcept = 0;
    // Reports whether the scope can currently hand focus to any control.
    [[nodiscard]] virtual bool has_focusable_target() const noexcept = 0;
    // Attempts to place focus on the scope's preferred initial target.
    virtual bool focus_first_available() = 0;
    [[nodiscard]] virtual UiControl* focused_target() const noexcept = 0;
    // Reports whether the scope can consume directional navigation for the action.
    [[nodiscard]] virtual bool can_navigate(UiAction action) const noexcept = 0;
    // Clears a focused target after a scroll container actually moves from gamepad input.
    // Scopes without a focused child can leave the default no-op implementation in place.
    virtual bool clear_focus_for_gamepad_scroll() { return false; }
    // Restores the focus cleared by clear_focus_for_gamepad_scroll().
    virtual bool restore_focus_after_gamepad_scroll() { return false; }
    // Checks whether a pointer position should move focus into this scope.
    [[nodiscard]] virtual bool contains_focus_point(int mouse_x,int mouse_y) const noexcept = 0;
};
}
