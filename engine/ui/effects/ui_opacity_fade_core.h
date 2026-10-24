#pragma once

#include "ui_opacity_common.h"

namespace elysia::ui::effects
{
enum class UiOpacityFadeMode
{
    FadeIn,
    FadeOut,
    FadeInOut
};

class UiOpacityFadeCore
{
public:
    UiOpacityFadeCore() noexcept = default;

    // Restores default playback settings and clears any in-flight fade state.
    void reset() noexcept
    {
        _mode = UiOpacityFadeMode::FadeIn;
        _state = State::Idle;
        _opacity = 255;
        _elapsed = 0.0;
        _hold_time = 0.0;
        _fade_in_duration = 1.0;
        _fade_out_duration = 1.0;
        _finished = false;
        _just_finished = false;
    }

    void configure_playback(UiOpacityFadeMode mode,double hold_time,double fade_in_duration,double fade_out_duration) noexcept
    {
        _mode = mode;
        _hold_time = hold_time > 0.0 ? hold_time : 0.0;
        _fade_in_duration = fade_in_duration > 0.0 ? fade_in_duration : 0.0;
        _fade_out_duration = fade_out_duration > 0.0 ? fade_out_duration : 0.0;
    }

    // Starts a new fade sequence from the configured initial opacity.
    void play() noexcept
    {
        _finished = false;
        _just_finished = false;
        _elapsed = 0.0;
        switch (_mode)
        {
        case UiOpacityFadeMode::FadeIn:
        case UiOpacityFadeMode::FadeInOut:
            _opacity = 0;
            _state = State::FadingIn;
            break;
        case UiOpacityFadeMode::FadeOut:
            _opacity = 255;
            begin_hold_or_fade_out();
            break;
        }
    }

    // Advances fade timing and reports whether playback completed during this tick.
    bool update(double delta_seconds) noexcept
    {
        if (_state == State::Idle || _state == State::Finished)
            return false;

        _just_finished = false;
        const double delta = delta_seconds > 0.0 ? delta_seconds : 0.0;

        switch (_state)
        {
        case State::FadingIn:
            _elapsed += delta;
            _opacity = lerp_opacity(0,255,ratio(_elapsed,_fade_in_duration));
            if (ratio(_elapsed,_fade_in_duration) >= 1.0)
            {
                _elapsed = 0.0;
                _opacity = 255;
                begin_hold_or_fade_out();
            }
            break;
        case State::Holding:
            _elapsed += delta;
            if (_elapsed >= _hold_time)
            {
                _elapsed = 0.0;
                if (_mode == UiOpacityFadeMode::FadeIn)
                    finish();
                else
                    _state = State::FadingOut;
            }
            break;
        case State::FadingOut:
            _elapsed += delta;
            _opacity = lerp_opacity(255,0,ratio(_elapsed,_fade_out_duration));
            if (ratio(_elapsed,_fade_out_duration) >= 1.0)
            {
                _opacity = 0;
                finish();
            }
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
        FadingIn,
        Holding,
        FadingOut,
        Finished
    };

    void begin_hold_or_fade_out() noexcept
    {
        if (_hold_time <= 0.0)
        {
            if (_mode == UiOpacityFadeMode::FadeIn)
                finish();
            else
            {
                _elapsed = 0.0;
                _state = State::FadingOut;
            }
            return;
        }
        _elapsed = 0.0;
        _state = State::Holding;
    }

    void finish() noexcept
    {
        _state = State::Finished;
        _finished = true;
        _just_finished = true;
    }

private:
    UiOpacityFadeMode _mode = UiOpacityFadeMode::FadeIn;
    State _state = State::Idle;
    std::uint8_t _opacity = 255;
    double _elapsed = 0.0;
    double _hold_time = 0.0;
    double _fade_in_duration = 1.0;
    double _fade_out_duration = 1.0;
    bool _finished = false;
    bool _just_finished = false;
};
}
