#pragma once

#include <optional>
#include <cstdint>

namespace elysia::ui::effects
{
enum class UiOpacityBlinkMode
{
    VisibleFirst,
    HiddenFirst
};

class UiOpacityBlinkCore
{
public:
    UiOpacityBlinkCore() noexcept = default;

    // Restores default playback settings and clears any in-flight blink state.
    void reset() noexcept
    {
        _mode = UiOpacityBlinkMode::VisibleFirst;
        _opacity = 255;
        _hold_time = 0.0;
        _visible_duration = 0.15;
        _hidden_duration = 0.15;
        _blink_cycles = std::nullopt;
        _completed_cycles = 0;
        _phase_remaining = 0.0;
        _is_playing = false;
        _finished = false;
        _just_finished = false;
        _hold_pending = false;
        _current_visible = true;
    }

    void configure_playback( UiOpacityBlinkMode mode,double hold_time,
        double visible_duration, double hidden_duration,std::optional<int> blink_cycles = std::nullopt) noexcept
    {
        _mode = mode;
        _hold_time = hold_time > 0.0 ? hold_time : 0.0;
        _visible_duration = visible_duration > 0.0 ? visible_duration : 0.0;
        _hidden_duration = hidden_duration > 0.0 ? hidden_duration : 0.0;
        _blink_cycles = blink_cycles;
    }

    // Starts a new blink sequence from the configured initial visible state.
    void play() noexcept
    {
        _is_playing = true;
        _finished = false;
        _just_finished = false;
        _completed_cycles = 0;
        _hold_pending = _hold_time > 0.0;
        _current_visible = start_visible();
        apply_current_phase();

        if (_blink_cycles && *_blink_cycles == 0)
        {
            finish();
            return;
        }

        if (_visible_duration <= 0.0 && _hidden_duration <= 0.0 && !_hold_pending)
        {
            finish();
            return;
        }

        _phase_remaining = _hold_pending ? _hold_time : current_duration();
        consume_instant_phase();
    }

    // Advances blink timing and reports whether playback completed during this tick.
    bool update(double delta_seconds) noexcept
    {
        if (!_is_playing)
            return false;

        _just_finished = false;
        double remaining = delta_seconds > 0.0 ? delta_seconds : 0.0;

        if (_phase_remaining <= 0.0)
            consume_instant_phase();

        while (_is_playing && remaining > 0.0)
        {
            if (_phase_remaining > remaining)
            {
                _phase_remaining -= remaining;
                break;
            }

            remaining -= _phase_remaining;
            _phase_remaining = 0.0;
            consume_timeout();
            consume_instant_phase();
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
    [[nodiscard]] bool start_visible() const noexcept
    {
        return _mode == UiOpacityBlinkMode::VisibleFirst;
    }

    [[nodiscard]] double current_duration() const noexcept
    {
        return _current_visible ? _visible_duration : _hidden_duration;
    }

    void apply_current_phase() noexcept
    {
        _opacity = _current_visible ? 255 : 0;
    }

    void consume_instant_phase() noexcept
    {
        while (_is_playing && _phase_remaining <= 0.0)
        {
            if (_visible_duration <= 0.0 && _hidden_duration <= 0.0)
            {
                _current_visible = start_visible();
                apply_current_phase();
                finish();
                return;
            }
            consume_timeout();
        }
    }

    void consume_timeout() noexcept
    {
        if (!_is_playing)
            return;

        if (_hold_pending)
        {
            _hold_pending = false;
            if (_visible_duration <= 0.0 && _hidden_duration <= 0.0)
            {
                _current_visible = start_visible();
                apply_current_phase();
                finish();
                return;
            }
            _current_visible = !_current_visible;
            apply_current_phase();
            _phase_remaining = current_duration();
            return;
        }

        const bool next_visible = !_current_visible;
        const bool returning_to_start = next_visible == start_visible();
        _current_visible = next_visible;
        apply_current_phase();

        if (returning_to_start && _blink_cycles.has_value())
        {
            ++_completed_cycles;
            if (_completed_cycles >= *_blink_cycles)
            {
                finish();
                return;
            }
        }

        _phase_remaining = current_duration();
    }

    void finish() noexcept
    {
        _is_playing = false;
        _finished = true;
        _just_finished = true;
        _hold_pending = false;
        _phase_remaining = 0.0;
    }

private:
    UiOpacityBlinkMode _mode = UiOpacityBlinkMode::VisibleFirst;
    std::uint8_t _opacity = 255;
    double _hold_time = 0.0;
    double _visible_duration = 0.15;
    double _hidden_duration = 0.15;
    std::optional<int> _blink_cycles = std::nullopt;
    int _completed_cycles = 0;
    double _phase_remaining = 0.0;
    bool _is_playing = false;
    bool _finished = false;
    bool _just_finished = false;
    bool _hold_pending = false;
    bool _current_visible = true;
};
}
