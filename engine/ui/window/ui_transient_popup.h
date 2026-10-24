#pragma once

#include "../input/ui_input_types.h"

#include <vector>

namespace elysia::core
{
struct UiRenderCommand;
}

namespace elysia::ui
{
class UiElement;
class UiWindow;

// A non-modal, window-rendered popup surface anchored by an ordinary UI control.
// Implementations retain popup ownership; UiWindow coordinates only z-order and input priority.
class UiTransientPopup
{
public:
    virtual ~UiTransientPopup() = default;

    [[nodiscard]] virtual UiElement& popup_owner() noexcept = 0;
    [[nodiscard]] virtual const UiElement& popup_owner() const noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    [[nodiscard]] virtual bool contains_popup_point(int mouse_x,int mouse_y) const noexcept = 0;
    virtual void close() noexcept = 0;
    // Clears implementation-side borrowed state when the registering window
    // unregisters or is being reset/destroyed.
    virtual void on_window_detached(UiWindow& window) noexcept = 0;
    virtual bool on_popup_input_event(const UiInputEvent& event) = 0;
    virtual void submit_popup_render_commands(
        std::vector<elysia::core::UiRenderCommand>& out_commands) const = 0;
};
}
