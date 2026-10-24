#include "gamepad_input_translator.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace elysia::input
{
namespace
{
constexpr float kTriggerPressedThreshold = 0.5f;
constexpr float kTriggerReleasedThreshold = 0.4f;
}

std::vector<RawInputEvent> GamepadInputTranslator::translate_event(const SDL_Event& event)
{
    std::vector<RawInputEvent> events;

    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        append_controller_button_events(
            events,
            event.gbutton.button,
            input_event_type(event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        );
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        switch (event.gaxis.axis)
        {
        case SDL_GAMEPAD_AXIS_LEFTX:
            append_axis_event(
                events,
                RawInputAxis::GamepadLeftX,
                normalize_stick_axis(event.gaxis.value)
            );
            break;

        case SDL_GAMEPAD_AXIS_LEFTY:
            append_axis_event(
                events,
                RawInputAxis::GamepadLeftY,
                normalize_stick_axis(event.gaxis.value)
            );
            break;

        case SDL_GAMEPAD_AXIS_RIGHTX:
            append_axis_event(
                events,
                RawInputAxis::GamepadRightX,
                normalize_stick_axis(event.gaxis.value)
            );
            break;

        case SDL_GAMEPAD_AXIS_RIGHTY:
            append_axis_event(
                events,
                RawInputAxis::GamepadRightY,
                normalize_stick_axis(event.gaxis.value)
            );
            break;

        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        {
            const float normalized_value = normalize_trigger_axis(event.gaxis.value);
            append_axis_event(events, RawInputAxis::GamepadLeftTrigger, normalized_value);

            if (!_left_trigger_pressed && normalized_value >= kTriggerPressedThreshold)
            {
                _left_trigger_pressed = true;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadLeftTriggerButton,
                    true
                );
            }
            else if (_left_trigger_pressed && normalized_value <= kTriggerReleasedThreshold)
            {
                _left_trigger_pressed = false;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadLeftTriggerButton,
                    false
                );
            }
            break;
        }

        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        {
            const float normalized_value = normalize_trigger_axis(event.gaxis.value);
            append_axis_event(events, RawInputAxis::GamepadRightTrigger, normalized_value);

            if (!_right_trigger_pressed && normalized_value >= kTriggerPressedThreshold)
            {
                _right_trigger_pressed = true;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadRightTriggerButton,
                    true
                );
            }
            else if (_right_trigger_pressed && normalized_value <= kTriggerReleasedThreshold)
            {
                _right_trigger_pressed = false;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadRightTriggerButton,
                    false
                );
            }
            break;
        }

        default:
            break;
        }
        break;

    default:
        break;
    }

    return events;
}

void GamepadInputTranslator::append_controller_button_events(
    std::vector<RawInputEvent>& events,
    std::uint8_t button,
    RawInputEventType type
) const
{
    switch (static_cast<SDL_GamepadButton>(button))
    {
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        append_event(events, RawInputControl::GamepadDPadLeft, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        append_event(events, RawInputControl::GamepadDPadRight, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        append_event(events, RawInputControl::GamepadDPadUp, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        append_event(events, RawInputControl::GamepadDPadDown, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_SOUTH:
        append_event(events, RawInputControl::GamepadSouth, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_EAST:
        append_event(events, RawInputControl::GamepadEast, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_WEST:
        append_event(events, RawInputControl::GamepadWest, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_NORTH:
        append_event(events, RawInputControl::GamepadNorth, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_BACK:
        append_event(events, RawInputControl::GamepadBack, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        append_event(events, RawInputControl::GamepadGuide, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_START:
        append_event(events, RawInputControl::GamepadStart, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        append_event(events, RawInputControl::GamepadLeftStick, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        append_event(events, RawInputControl::GamepadRightStick, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        append_event(events, RawInputControl::GamepadLeftShoulder, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        append_event(events, RawInputControl::GamepadRightShoulder, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_MISC1:
        append_event(events, RawInputControl::GamepadMisc1, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
        append_event(events, RawInputControl::GamepadPaddle1, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
        append_event(events, RawInputControl::GamepadPaddle2, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
        append_event(events, RawInputControl::GamepadPaddle3, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
        append_event(events, RawInputControl::GamepadPaddle4, type, InputDevice::Gamepad);
        break;
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
        append_event(events, RawInputControl::GamepadTouchpad, type, InputDevice::Gamepad);
        break;
    default:
        break;
    }
}

void GamepadInputTranslator::append_axis_event(
    std::vector<RawInputEvent>& events,
    RawInputAxis axis,
    float normalized_value
) const
{
    RawInputEvent input_event;
    input_event.type = RawInputEventType::AxisChanged;
    input_event.device = InputDevice::Gamepad;
    input_event.axis = axis;
    input_event.axis_value = normalized_value;
    events.push_back(input_event);
}

void GamepadInputTranslator::append_trigger_virtual_button_event(
    std::vector<RawInputEvent>& events,
    RawInputControl control,
    bool pressed
) const
{
    append_event(
        events,
        control,
        pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased,
        InputDevice::Gamepad
    );
}

float GamepadInputTranslator::normalize_stick_axis(std::int16_t value) const
{
    return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
}

float GamepadInputTranslator::normalize_trigger_axis(std::int16_t value) const
{
    return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
}

}
