#pragma once

#include <optional>
#include "ui_opacity_common.h"

namespace elysia::ui::effects
{
enum class UiOpacityPulseMode
{
    MinToMax,
    MaxToMin
};

class UiOpacityPulseCore
{
public:
    UiOpacityPulseCore() noexcept = default;

    // Restores default playback settings and clears any in-flight pulse state.
    void reset() noexcept
    {
        _mode = UiOpacityPulseMode::MinToMax;
        _state = State::Idle;
        _opacity = 255;
        _elapsed = 0.0;
        _hold_time = 0.0;
        _pulse_in_duration = 1.0;
        _pulse_out_duration = 1.0;
        _pulse_cycles = std::nullopt;
        _completed_cycles = 0;
        _min_opacity = 96;
        _max_opacity = 255;
        _finished = false;
        _just_finished = false;
    }

    void configure_playback(UiOpacityPulseMode mode,
        double hold_time,double pulse_in_duration,double pulse_out_duration,
        std::optional<int> pulse_cycles = std::nullopt,std::uint8_t min_opacity = 96,std::uint8_t max_opacity = 255) noexcept
    {
        _mode = mode;
        _hold_time = hold_time > 0.0 ? hold_time : 0.0;
        _pulse_in_duration = pulse_in_duration > 0.0 ? pulse_in_duration : 0.0;
        _pulse_out_duration = pulse_out_duration > 0.0 ? pulse_out_duration : 0.0;
        _pulse_cycles = pulse_cycles;
        if (min_opacity <= max_opacity)
        {
            _min_opacity = min_opacity;
            _max_opacity = max_opacity;
        }
        else
        {
            _min_opacity = max_opacity;
            _max_opacity = min_opacity;
        }
        _opacity = _max_opacity;
    }

    // Starts a new pulse sequence from the configured edge of the opacity range.
    void play() noexcept
    {
        _finished = false;
        _just_finished = false;
        _completed_cycles = 0;
        _elapsed = 0.0;
        if (started_from_min())
        {
            _opacity = _min_opacity;
            _state = State::PulsingUp;
        }
        else
        {
            _opacity = _max_opacity;
            _state = State::PulsingDown;
        }
        if (_pulse_cycles && *_pulse_cycles == 0)
            finish();
    }

    // Advances pulse timing and reports whether playback completed during this tick.
    bool update(double delta_seconds) noexcept
    {
        if (_state == State::Idle || _state == State::Finished)
            return false;
        _just_finished = false;
        const double delta = delta_seconds > 0.0 ? delta_seconds : 0.0;
        switch (_state)
        {
        case State::PulsingUp:
            update_pulse_up(delta);
            break;
        case State::HoldingMax:
        case State::HoldingMin:
            update_hold(delta);
            break;
        case State::PulsingDown:
            update_pulse_down(delta);
            break;
        default:
            break;
        }
        return _just_finished;
    }

    [[nodiscard]] std::uint8_t opacity() const noexcept
    {
        return _opacity;
    }

    [[nodiscard]] bool is_finished() const noexcept
    {
        return _finished;
    }

private:
    enum class State
    {
        Idle,
        PulsingUp,
        HoldingMax,
        PulsingDown,
        HoldingMin,
        Finished
    };

    void update_pulse_up(double delta) noexcept
    {
        _elapsed += delta;
        _opacity = lerp_opacity(_min_opacity,_max_opacity,ease_in_out(ratio(_elapsed,_pulse_in_duration)));
        if (ratio(_elapsed,_pulse_in_duration) < 1.0)
            return;
        _elapsed = 0.0;
        _opacity = _max_opacity;
        if (complete_cycle_if_needed(true))
        {
            finish();
            return;
        }
        start_hold(true);
    }

    void update_pulse_down(double delta) noexcept
    {
        _elapsed += delta;
        _opacity = lerp_opacity(_max_opacity,_min_opacity,ease_in_out(ratio(_elapsed,_pulse_out_duration)));
        if (ratio(_elapsed,_pulse_out_duration) < 1.0)
            return;
        _elapsed = 0.0;
        _opacity = _min_opacity;
        if (complete_cycle_if_needed(false))
        {
            finish();
            return;
        }
        start_hold(false);
    }

    void update_hold(double delta) noexcept
    {
        _elapsed += delta;
        if (_elapsed < _hold_time)
            return;
        _elapsed = 0.0;
        _state = _state == State::HoldingMax ? State::PulsingDown : State::PulsingUp;
    }

    void start_hold(bool at_max) noexcept
    {
        if (_hold_time <= 0.0)
        {
            _state = at_max ? State::PulsingDown : State::PulsingUp;
            return;
        }
        _state = at_max ? State::HoldingMax : State::HoldingMin;
    }

    void finish() noexcept
    {
        _state = State::Finished;
        _finished = true;
        _just_finished = true;
    }

    [[nodiscard]] bool started_from_min() const noexcept
    {
        return _mode == UiOpacityPulseMode::MinToMax;
    }

    [[nodiscard]] bool complete_cycle_if_needed(bool reached_max) noexcept
    {
        const bool cycle_completed = (started_from_min() && !reached_max) || (!started_from_min() && reached_max);
        if (!cycle_completed || !_pulse_cycles.has_value())
            return false;
        ++_completed_cycles;
        return _completed_cycles >= *_pulse_cycles;
    }

private:
    UiOpacityPulseMode _mode = UiOpacityPulseMode::MinToMax;
    State _state = State::Idle;
    std::uint8_t _opacity = 255;
    double _elapsed = 0.0;
    double _hold_time = 0.0;
    double _pulse_in_duration = 1.0;
    double _pulse_out_duration = 1.0;
    std::optional<int> _pulse_cycles = std::nullopt;
    int _completed_cycles = 0;
    std::uint8_t _min_opacity = 96;
    std::uint8_t _max_opacity = 255;
    bool _finished = false;
    bool _just_finished = false;
};
}
