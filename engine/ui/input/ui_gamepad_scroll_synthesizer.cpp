#include "ui_gamepad_scroll_synthesizer.h"

#include <algorithm>
#include <cmath>

namespace elysia::ui
{
std::optional<UiInputEvent> UiGamepadScrollSynthesizer::synthesize(const elysia::input::RawInputFrame& input)
{
    if (input.active_device != elysia::input::InputDevice::Gamepad || input.device_switched_this_frame)
    {
        reset();
        return std::nullopt;
    }

    const float normalized_x = normalize_axis(
        input.state.axis_value(elysia::input::RawInputAxis::GamepadLeftX)
    );
    const float normalized_y = normalize_axis(
        input.state.axis_value(elysia::input::RawInputAxis::GamepadLeftY)
    );

    if (normalized_x == 0.0f)
        _scroll_accumulator_x = 0.0f;
    else
        _scroll_accumulator_x += (-normalized_x) * 0.75f;

    if (normalized_y == 0.0f)
        _scroll_accumulator_y = 0.0f;
    else
        _scroll_accumulator_y += (-normalized_y) * 0.75f;

    const int wheel_x = take_wheel_steps(_scroll_accumulator_x);
    const int wheel_y = take_wheel_steps(_scroll_accumulator_y);
    if (wheel_x == 0 && wheel_y == 0)
    {
        return std::nullopt;
    }

    UiInputEvent scroll_event;
    scroll_event.type = UiInputEventType::MouseWheel;
    scroll_event.device = elysia::input::InputDevice::Gamepad;
    scroll_event.wheel_x = wheel_x;
    scroll_event.wheel_y = wheel_y;
    return scroll_event;
}

void UiGamepadScrollSynthesizer::reset()
{
    _scroll_accumulator_x = 0.0f;
    _scroll_accumulator_y = 0.0f;
}

int UiGamepadScrollSynthesizer::take_wheel_steps(float& accumulator) noexcept
{
    int steps = 0;
    if (accumulator >= 1.0f)
        steps = static_cast<int>(std::floor(accumulator));
    else if (accumulator <= -1.0f)
        steps = static_cast<int>(std::ceil(accumulator));

    steps = std::clamp(steps,-3,3);
    accumulator -= static_cast<float>(steps);
    return steps;
}

float UiGamepadScrollSynthesizer::normalize_axis(float axis_value) const
{
    const float deadzone = 0.22f;
    const float abs_value = std::fabs(axis_value);
    if (abs_value <= deadzone)
    {
        return 0.0f;
    }

    const float normalized = (abs_value - deadzone) / (1.0f - deadzone);
    return axis_value < 0.0f ? -normalized : normalized;
}

}
