#pragma once

#include "../../input/raw_input_frame.h"
#include "ui_gamepad_scroll_synthesizer.h"
#include "ui_input_frame.h"

#include <vector>

namespace elysia::ui
{
class UiInputRouter
{
public:
    // Builds the per-frame UI state snapshot from raw device input.
    UiInputFrame route_frame(const elysia::input::RawInputFrame& raw_input) const;
    // Maps one raw input event into zero or more UI-facing events.
    std::vector<UiInputEvent> route_event(const elysia::input::RawInputEvent& raw_event) const;
    // Generates synthetic UI events such as gamepad-driven scrolling.
    std::vector<UiInputEvent> synthesize_events(const elysia::input::RawInputFrame& raw_input);
    // Clears router state that should not persist across interaction sessions.
    void reset_transient_state();

private:
    [[nodiscard]] bool is_action_pressed(const elysia::input::RawInputState& state, UiAction action) const;
    [[nodiscard]] bool is_action_just_pressed(const elysia::input::RawInputState& state, UiAction action) const;
    [[nodiscard]] bool is_action_just_released(const elysia::input::RawInputState& state, UiAction action) const;
    [[nodiscard]] UiAction action_from_control(elysia::input::RawInputControl control) const;

private:
    UiGamepadScrollSynthesizer _gamepad_scroll_synthesizer;
};
}
