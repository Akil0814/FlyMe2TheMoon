#pragma once
#include "input_snapshot.h"
#include <cmath>
namespace elysia::input
{
inline InputCapture capture_for(InputDevice device)
{
    if (device == InputDevice::Keyboard)
        return InputCapture::Keyboard;
    if (device == InputDevice::Mouse)
        return InputCapture::Pointer;
    if (device == InputDevice::Gamepad)
        return InputCapture::Gamepad;
    return InputCapture::None;
}
inline bool captured(InputCapture mask, InputDevice device)
{
    return captures_input(mask, capture_for(device));
}
class InputSuppression
{
  public:
    void block(const RawInputState &state, InputCapture mask)
    {
        for (int i = 1; i < int(RawInputControl::Count); ++i)
        {
            auto c = static_cast<RawInputControl>(i);
            auto device = is_keyboard_control(c)       ? InputDevice::Keyboard
                          : is_mouse_button_control(c) ? InputDevice::Mouse
                                                       : InputDevice::Gamepad;
            if (captured(mask, device) && state.is_pressed(c))
                _blocked.set_pressed(c, true);
        }
        if (captured(mask, InputDevice::Gamepad))
            for (int i = 1; i < int(RawInputAxis::Count); ++i)
            {
                auto a = static_cast<RawInputAxis>(i);
                if (std::abs(state.axis_value(a)) > 0.2f)
                    _blocked.set_axis(a, 1.f);
            }
    }
    void block(const RawInputEvent &event)
    {
        if (event.type == RawInputEventType::ControlPressed)
            _blocked.set_pressed(event.control, true);
        if (event.type == RawInputEventType::AxisChanged && std::abs(event.axis_value) > 0.2f)
            _blocked.set_axis(event.axis, 1.f);
    }
    // Advance only from physical state, in event order. Filtering is a pure query.
    void observe(const RawInputState &physical, InputCapture mask = InputCapture::None)
    {
        for (int i = 1; i < int(RawInputControl::Count); ++i)
        {
            auto c = static_cast<RawInputControl>(i);
            if (!physical.is_pressed(c))
                _blocked.set_pressed(c, false);
        }
        for (int i = 1; i < int(RawInputAxis::Count); ++i)
        {
            auto a = static_cast<RawInputAxis>(i);
            if (std::abs(physical.axis_value(a)) <= 0.2f)
                _blocked.set_axis(a, 0.f);
        }
        block(physical, mask);
    }
    [[nodiscard]] RawInputState filter(const RawInputState &physical,
                                       InputCapture mask = InputCapture::None) const
    {
        RawInputState out = physical;
        for (int i = 1; i < int(RawInputControl::Count); ++i)
        {
            auto c = static_cast<RawInputControl>(i);
            auto device = is_keyboard_control(c)       ? InputDevice::Keyboard
                          : is_mouse_button_control(c) ? InputDevice::Mouse
                                                       : InputDevice::Gamepad;
            if (_blocked.is_pressed(c) || captured(mask, device))
                out.clear_control(c);
        }
        for (int i = 1; i < int(RawInputAxis::Count); ++i)
        {
            auto a = static_cast<RawInputAxis>(i);
            if (_blocked.axis_value(a) != 0 || captured(mask, InputDevice::Gamepad))
                out.set_axis(a, 0.f);
        }
        return out;
    }

  private:
    RawInputState _blocked;
};
inline constexpr InputCapture AllInputCapture =
    InputCapture::Keyboard | InputCapture::Pointer | InputCapture::Gamepad;
} // namespace elysia::input
