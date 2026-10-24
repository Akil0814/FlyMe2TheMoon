#pragma once
#include "raw_input_frame.h"
#include "input_capture.h"
#include <vector>
#include <algorithm>
namespace elysia::input
{
struct InputSourceFrame
{
    InputSourceId source;
    RawInputState initial_state;
    RawInputFrame frame;
};
struct InputSnapshot
{
    std::vector<InputSourceFrame> sources;
    std::vector<RawInputEvent> events;
    std::vector<InputSourceId> connected, removed;
    bool focus_lost = false;
    InputCapture capture = InputCapture::None;
    [[nodiscard]] const InputSourceFrame *find(InputSourceId id) const
    {
        for (const auto &source : sources)
            if (source.source == id)
                return &source;
        return nullptr;
    }
};
inline void apply_raw_event(RawInputState &state, const RawInputEvent &event)
{
    if (event.type == RawInputEventType::ControlPressed)
        state.set_pressed(event.control, true);
    if (event.type == RawInputEventType::ControlReleased)
        state.set_pressed(event.control, false);
    if (event.type == RawInputEventType::AxisChanged)
        state.set_axis(event.axis, event.axis_value);
}
inline void merge_raw_state(RawInputState &out, const RawInputState &state)
{
    std::array<bool, static_cast<std::size_t>(RawInputControl::Count)> current{};
    for (int i = 1; i < int(RawInputControl::Count); ++i)
    {
        auto c = static_cast<RawInputControl>(i);
        current[i] = out.is_pressed(c) || state.is_pressed(c);
        const bool out_previous = out.is_just_released(c) || (out.is_pressed(c) && !out.is_just_pressed(c));
        const bool source_previous =
            state.is_just_released(c) || (state.is_pressed(c) && !state.is_just_pressed(c));
        out.set_pressed(c, out_previous || source_previous);
    }
    out.begin_frame();
    for (int i = 1; i < int(RawInputControl::Count); ++i)
        out.set_pressed(static_cast<RawInputControl>(i), current[i]);
    for (int i = 1; i < int(RawInputAxis::Count); ++i)
    {
        auto a = static_cast<RawInputAxis>(i);
        out.set_axis(a, std::clamp(out.axis_value(a) + state.axis_value(a), -1.f, 1.f));
    }
}
} // namespace elysia::input
