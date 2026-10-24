#include "ui_input_router.h"

namespace
{
struct UiBinding
{
    elysia::ui::UiAction action;
    elysia::input::RawInputControl control;
};

constexpr UiBinding kUiBindings[] = {
    { elysia::ui::UiAction::NavigateLeft, elysia::input::RawInputControl::KeyLeft },
    { elysia::ui::UiAction::NavigateRight, elysia::input::RawInputControl::KeyRight },
    { elysia::ui::UiAction::NavigateUp, elysia::input::RawInputControl::KeyUp },
    { elysia::ui::UiAction::NavigateDown, elysia::input::RawInputControl::KeyDown },
    { elysia::ui::UiAction::Confirm, elysia::input::RawInputControl::KeyEnter },
    { elysia::ui::UiAction::Confirm, elysia::input::RawInputControl::KeyNumpadEnter },
    { elysia::ui::UiAction::Confirm, elysia::input::RawInputControl::GamepadSouth },
    { elysia::ui::UiAction::Cancel, elysia::input::RawInputControl::KeyEscape },
    { elysia::ui::UiAction::Cancel, elysia::input::RawInputControl::GamepadEast },
    { elysia::ui::UiAction::Tab, elysia::input::RawInputControl::KeyTab },
    { elysia::ui::UiAction::Backspace, elysia::input::RawInputControl::KeyBackspace },
    { elysia::ui::UiAction::DeleteKey, elysia::input::RawInputControl::KeyDelete },
    { elysia::ui::UiAction::Home, elysia::input::RawInputControl::KeyHome },
    { elysia::ui::UiAction::End, elysia::input::RawInputControl::KeyEnd },
    { elysia::ui::UiAction::PageUp, elysia::input::RawInputControl::KeyPageUp },
    { elysia::ui::UiAction::PageDown, elysia::input::RawInputControl::KeyPageDown },
    { elysia::ui::UiAction::NavigateLeft, elysia::input::RawInputControl::GamepadDPadLeft },
    { elysia::ui::UiAction::NavigateRight, elysia::input::RawInputControl::GamepadDPadRight },
    { elysia::ui::UiAction::NavigateUp, elysia::input::RawInputControl::GamepadDPadUp },
    { elysia::ui::UiAction::NavigateDown, elysia::input::RawInputControl::GamepadDPadDown }
};
}

namespace elysia::ui
{
UiInputFrame UiInputRouter::route_frame(const elysia::input::RawInputFrame& raw_input) const
{
    UiInputFrame ui_input;
    ui_input.active_device = raw_input.active_device;
    ui_input.device_switched_this_frame = raw_input.device_switched_this_frame;

    for (int action_index = static_cast<int>(UiAction::None) + 1;
        action_index < static_cast<int>(UiAction::Count);
        ++action_index)
    {
        const UiAction action = static_cast<UiAction>(action_index);
        ui_input.state.set(
            action,
            is_action_pressed(raw_input.state, action),
            is_action_just_pressed(raw_input.state, action),
            is_action_just_released(raw_input.state, action)
        );
    }

    return ui_input;
}

std::vector<UiInputEvent> UiInputRouter::route_event(const elysia::input::RawInputEvent& raw_event) const
{
    std::vector<UiInputEvent> events;

    switch (raw_event.type)
    {
    case elysia::input::RawInputEventType::ControlPressed:
    case elysia::input::RawInputEventType::ControlReleased:
    {
        if (raw_event.device == elysia::input::InputDevice::Mouse
            && raw_event.control == elysia::input::RawInputControl::MouseLeft)
        {
            UiInputEvent event;
            event.type = raw_event.type == elysia::input::RawInputEventType::ControlPressed
                ? UiInputEventType::PointerPressed
                : UiInputEventType::PointerReleased;
            event.device = raw_event.device;
            event.control = raw_event.control;
            event.mouse_x = raw_event.mouse_x;
            event.mouse_y = raw_event.mouse_y;
            events.push_back(event);
            break;
        }

        const UiAction action = action_from_control(raw_event.control);
        if (action == UiAction::None)
        {
            return events;
        }

        UiInputEvent event;
        event.action = action;
        event.type = raw_event.type == elysia::input::RawInputEventType::ControlPressed
            ? UiInputEventType::ActionPressed
            : UiInputEventType::ActionReleased;
        event.device = raw_event.device;
        event.control = raw_event.control;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::MouseMoved:
    {
        UiInputEvent event;
        event.type = UiInputEventType::MouseMoved;
        event.device = raw_event.device;
        event.mouse_x = raw_event.mouse_x;
        event.mouse_y = raw_event.mouse_y;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::MouseWheel:
    {
        UiInputEvent event;
        event.type = UiInputEventType::MouseWheel;
        event.device = raw_event.device;
        event.mouse_x = raw_event.mouse_x;
        event.mouse_y = raw_event.mouse_y;
        event.wheel_x = raw_event.wheel_x;
        event.wheel_y = raw_event.wheel_y;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::TextInput:
    {
        UiInputEvent event;
        event.type = UiInputEventType::TextInput;
        event.device = raw_event.device;
        event.text = raw_event.text;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::TextEditing:
    {
        UiInputEvent event;
        event.type = UiInputEventType::TextEditing;
        event.device = raw_event.device;
        event.text = raw_event.text;
        event.composition_start = raw_event.composition_start;
        event.composition_length = raw_event.composition_length;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::AxisChanged:
    {
        UiInputEvent event;
        event.type = UiInputEventType::AxisChanged;
        event.device = raw_event.device;
        event.axis = raw_event.axis;
        event.axis_value = raw_event.axis_value;
        events.push_back(event);
        break;
    }

    case elysia::input::RawInputEventType::None:
    default:
        break;
    }

    return events;
}

std::vector<UiInputEvent> UiInputRouter::synthesize_events(const elysia::input::RawInputFrame& raw_input)
{
    std::vector<UiInputEvent> events;

    const auto synthesized_event = _gamepad_scroll_synthesizer.synthesize(raw_input);
    if (synthesized_event)
    {
        events.push_back(*synthesized_event);
    }

    return events;
}

void UiInputRouter::reset_transient_state()
{
    _gamepad_scroll_synthesizer.reset();
}

bool UiInputRouter::is_action_pressed(const elysia::input::RawInputState& state, UiAction action) const
{
    for (const UiBinding& binding : kUiBindings)
    {
        if (binding.action == action && state.is_pressed(binding.control))
        {
            return true;
        }
    }

    return false;
}

bool UiInputRouter::is_action_just_pressed(const elysia::input::RawInputState& state, UiAction action) const
{
    for (const UiBinding& binding : kUiBindings)
    {
        if (binding.action == action && state.is_just_pressed(binding.control))
        {
            return true;
        }
    }

    return false;
}

bool UiInputRouter::is_action_just_released(const elysia::input::RawInputState& state, UiAction action) const
{
    for (const UiBinding& binding : kUiBindings)
    {
        if (binding.action == action && state.is_just_released(binding.control))
        {
            return true;
        }
    }

    return false;
}

UiAction UiInputRouter::action_from_control(elysia::input::RawInputControl control) const
{
    for (const UiBinding& binding : kUiBindings)
    {
        if (binding.control == control)
        {
            return binding.action;
        }
    }

    return UiAction::None;
}
}
