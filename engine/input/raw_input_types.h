#pragma once

#include <SDL3/SDL.h>

#include "input_types.h"
#include "input_source.h"

#include <cstdint>
#include <string>

namespace elysia::input
{
enum class RawInputControl
{
    None = 0,

    KeyA,
    KeyB,
    KeyC,
    KeyD,
    KeyE,
    KeyF,
    KeyG,
    KeyH,
    KeyI,
    KeyJ,
    KeyK,
    KeyL,
    KeyM,
    KeyN,
    KeyO,
    KeyP,
    KeyQ,
    KeyR,
    KeyS,
    KeyT,
    KeyU,
    KeyV,
    KeyW,
    KeyX,
    KeyY,
    KeyZ,

    Key0,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
    Key6,
    Key7,
    Key8,
    Key9,

    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown,
    KeyEnter,
    KeyNumpadEnter,
    KeyEscape,
    KeySpace,
    KeyInsert,
    KeyPageUp,
    KeyPageDown,
    KeyLeftShift,
    KeyRightShift,
    KeyLeftCtrl,
    KeyRightCtrl,
    KeyLeftAlt,
    KeyRightAlt,
    KeyBackspace,
    KeyDelete,
    KeyHome,
    KeyEnd,
    KeyTab,
    KeyMinus,
    KeyEquals,
    KeyLeftBracket,
    KeyRightBracket,
    KeySemicolon,
    KeyApostrophe,
    KeyComma,
    KeyPeriod,
    KeySlash,
    KeyBackslash,
    KeyGrave,
    KeyF1,
    KeyF2,
    KeyF3,
    KeyF4,
    KeyF5,
    KeyF6,
    KeyF7,
    KeyF8,
    KeyF9,
    KeyF10,
    KeyF11,
    KeyF12,

    MouseLeft,
    MouseRight,
    MouseMiddle,
    MouseX1,
    MouseX2,

    GamepadSouth,
    GamepadEast,
    GamepadWest,
    GamepadNorth,
    GamepadBack,
    GamepadGuide,
    GamepadStart,
    GamepadLeftStick,
    GamepadRightStick,
    GamepadLeftShoulder,
    GamepadRightShoulder,
    GamepadDPadLeft,
    GamepadDPadRight,
    GamepadDPadUp,
    GamepadDPadDown,
    GamepadLeftTriggerButton,
    GamepadRightTriggerButton,
    GamepadMisc1,
    GamepadPaddle1,
    GamepadPaddle2,
    GamepadPaddle3,
    GamepadPaddle4,
    GamepadTouchpad,

    AnyKey,
    AnyMouseButton,
    AnyGamepadButton,
    AnyControl,

    Count
};

[[nodiscard]] constexpr bool is_keyboard_control(RawInputControl control) noexcept
{
    return control >= RawInputControl::KeyA && control <= RawInputControl::KeyF12;
}

[[nodiscard]] constexpr bool is_mouse_button_control(RawInputControl control) noexcept
{
    return control >= RawInputControl::MouseLeft && control <= RawInputControl::MouseX2;
}

[[nodiscard]] constexpr bool is_gamepad_button_control(RawInputControl control) noexcept
{
    return control >= RawInputControl::GamepadSouth && control <= RawInputControl::GamepadTouchpad;
}

[[nodiscard]] constexpr bool matches_control(RawInputControl expected, RawInputControl actual) noexcept
{
    switch (expected)
    {
    case RawInputControl::AnyKey:
        return is_keyboard_control(actual);
    case RawInputControl::AnyMouseButton:
        return is_mouse_button_control(actual);
    case RawInputControl::AnyGamepadButton:
        return is_gamepad_button_control(actual);
    case RawInputControl::AnyControl:
        return is_keyboard_control(actual) || is_mouse_button_control(actual) ||
               is_gamepad_button_control(actual);
    default:
        return expected == actual;
    }
}

enum class RawInputAxis
{
    None = 0,
    GamepadLeftX,
    GamepadLeftY,
    GamepadRightX,
    GamepadRightY,
    GamepadLeftTrigger,
    GamepadRightTrigger,
    Count
};

enum class RawInputEventType
{
    None = 0,
    ControlPressed,
    ControlReleased,
    MouseMoved,
    MouseWheel,
    TextInput,
    TextEditing,
    AxisChanged
};

struct RawInputEvent
{
    RawInputControl control = RawInputControl::None;
    RawInputAxis axis = RawInputAxis::None;
    RawInputEventType type = RawInputEventType::None;
    InputDevice device = InputDevice::Unknown;
    SDL_Keycode keycode = SDLK_UNKNOWN;
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    std::uint8_t mouse_button = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    int mouse_delta_x = 0;
    int mouse_delta_y = 0;
    float wheel_x = 0;
    float wheel_y = 0;
    int composition_start = 0;
    int composition_length = 0;
    // Stick axes: [-1.0f, 1.0f]
    // Trigger axes: [0.0f, 1.0f]
    float axis_value = 0.0f;
    std::string text;
    InputSourceId source{};
    // Assigned by Scene for exact operation consumption within one snapshot.
    std::uint64_t routing_id = 0;
};

} // namespace elysia::input
