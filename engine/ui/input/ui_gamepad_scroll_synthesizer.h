#pragma once

#include "ui_input_types.h"

#include "../../input/raw_input_frame.h"

#include <optional>

namespace elysia::ui
{
class UiGamepadScrollSynthesizer
{
public:
    // Converts held gamepad axis input into repeated mouse-wheel-style UI events.
    std::optional<UiInputEvent> synthesize(const elysia::input::RawInputFrame& input);
    // Clears accumulated axis state so scrolling does not leak across screens.
    void reset();

private:
    [[nodiscard]] float normalize_axis(float axis_value) const;
    static int take_wheel_steps(float& accumulator) noexcept;

private:
    float _scroll_accumulator_x = 0.0f;
    float _scroll_accumulator_y = 0.0f;
};

}
