#pragma once

#include "ui_input_types.h"

#include <array>
#include <cstddef>

namespace elysia::ui
{
// Per-frame action snapshot. Edge flags describe only the frame in which this state was produced.
class UiInputState
{
public:
    // Records current, just-pressed, and just-released state for one UI action.
    void set(UiAction action, bool pressed, bool just_pressed, bool just_released)
    {
        if (!is_trackable_action(action))
        {
            return;
        }

        const std::size_t i = index(action);
        _pressed[i] = pressed;
        _just_pressed[i] = just_pressed;
        _just_released[i] = just_released;
    }

    [[nodiscard]] bool is_pressed(UiAction action) const
    {
        return get(_pressed, action);
    }

    [[nodiscard]] bool is_just_pressed(UiAction action) const
    {
        return get(_just_pressed, action);
    }

    [[nodiscard]] bool is_just_released(UiAction action) const
    {
        return get(_just_released, action);
    }

private:
    static constexpr bool is_trackable_action(UiAction action)
    {
        return action != UiAction::None && action != UiAction::Count;
    }

    static constexpr std::size_t index(UiAction action)
    {
        return static_cast<std::size_t>(action);
    }

    [[nodiscard]] bool get(
        const std::array<bool, static_cast<std::size_t>(UiAction::Count)>& values,
        UiAction action
    ) const
    {
        if (!is_trackable_action(action))
        {
            return false;
        }

        return values[index(action)];
    }

private:
    std::array<bool, static_cast<std::size_t>(UiAction::Count)> _pressed{};
    std::array<bool, static_cast<std::size_t>(UiAction::Count)> _just_pressed{};
    std::array<bool, static_cast<std::size_t>(UiAction::Count)> _just_released{};
};

}
