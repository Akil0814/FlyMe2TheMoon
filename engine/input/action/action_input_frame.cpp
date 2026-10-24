#include "action_input_frame.h"

#include <cmath>

namespace elysia::input
{
const ActionInputFrame::State *ActionInputFrame::find(const InputActionId &action) const
{
    const auto iter = _states.find(action);
    return iter == _states.end() ? nullptr : &iter->second;
}

bool ActionInputFrame::contains(const InputActionId &action) const
{
    return find(action) != nullptr;
}

bool ActionInputFrame::actuated(const State &state, const InputActionValue &value)
{
    switch (state.value_type)
    {
    case InputActionValueType::Button:
        return value.x >= state.actuation_threshold;
    case InputActionValueType::Axis1D:
        return std::fabs(value.x) >= state.actuation_threshold;
    case InputActionValueType::Axis2D:
        return value.x * value.x + value.y * value.y >= state.actuation_threshold * state.actuation_threshold;
    }
    return false;
}

bool ActionInputFrame::is_pressed(const InputActionId &action) const
{
    const State *state = find(action);
    return state && actuated(*state, state->current);
}

bool ActionInputFrame::is_just_pressed(const InputActionId &action) const
{
    const State *state = find(action);
    return state && actuated(*state, state->current) && !actuated(*state, state->previous);
}

bool ActionInputFrame::is_just_released(const InputActionId &action) const
{
    const State *state = find(action);
    return state && !actuated(*state, state->current) && actuated(*state, state->previous);
}

float ActionInputFrame::axis1d(const InputActionId &action) const
{
    const State *state = find(action);
    return state && state->value_type == InputActionValueType::Axis1D ? state->current.x : 0.0f;
}

elysia::core::Vector2 ActionInputFrame::axis2d(const InputActionId &action) const
{
    const State *state = find(action);
    return state && state->value_type == InputActionValueType::Axis2D ? state->current.vector2()
                                                                      : elysia::core::Vector2::zero();
}

InputActionValue ActionInputFrame::value(const InputActionId &action) const
{
    const State *state = find(action);
    return state ? state->current : InputActionValue{};
}
} // namespace elysia::input

#include <cmath>
bool elysia::input::ActionInputFrame::finite() const
{
    for (const auto &[id, s] : _states)
        if (!std::isfinite(s.current.x) || !std::isfinite(s.current.y) || !std::isfinite(s.previous.x) ||
            !std::isfinite(s.previous.y) || !std::isfinite(s.actuation_threshold))
            return false;
    return true;
}
