#pragma once

#include "../../input/raw_input_types.h"

#include <string>

namespace elysia::ui
{
// Device-independent actions consumed by focusable UI controls.
enum class UiAction
{
    None = 0,
    NavigateLeft,
    NavigateRight,
    NavigateUp,
    NavigateDown,
    Confirm,
    Cancel,
    Tab,
    Backspace,
    DeleteKey,
    Home,
    End,
    PageUp,
    PageDown,
    Count
};

// Event phase or payload family after raw keyboard, mouse, and gamepad input is normalized.
enum class UiInputEventType
{
    None = 0,
    ActionPressed,
    ActionReleased,
    MouseMoved,
    PointerPressed,
    PointerReleased,
    MouseWheel,
    TextInput,
    TextEditing,
    AxisChanged
};

// Normalized UI event payload routed to controls after raw input mapping.
struct UiInputEvent
{
    // Only fields relevant to type are meaningful; unused payload members retain neutral defaults.
    UiAction action = UiAction::None;
    UiInputEventType type = UiInputEventType::None;
    elysia::input::InputDevice device = elysia::input::InputDevice::Unknown;
    elysia::input::RawInputControl control = elysia::input::RawInputControl::None;
    elysia::input::RawInputAxis axis = elysia::input::RawInputAxis::None;
    int mouse_x = 0;
    int mouse_y = 0;
    float wheel_x = 0;
    float wheel_y = 0;
    int composition_start = 0;
    int composition_length = 0;
    float axis_value = 0.0f;
    std::string text;
};

}
