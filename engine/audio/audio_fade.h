#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

namespace elysia::audio
{
inline double audio_delta(double seconds)
{
    return std::isfinite(seconds) ? std::max(0.0,seconds) : 0.0;
}

class AudioFade
{
public:
    void reset(double gain = 1.0) { _gain = _from = _target = gain; _elapsed = _duration = 0.0; }
    void start(double target,std::chrono::milliseconds duration)
    {
        _from = _gain;
        _target = target;
        _elapsed = 0.0;
        _duration = std::max(0.0,duration.count() / 1000.0);
        if (_duration == 0.0) _gain = _target;
    }
    void update(double seconds)
    {
        _elapsed = std::min(_duration,_elapsed + audio_delta(seconds));
        if (_duration > 0.0)
            _gain = _from + (_target - _from) * (_elapsed / _duration);
    }
    [[nodiscard]] double gain() const { return _gain; }
    [[nodiscard]] bool finished() const { return _elapsed >= _duration; }
private:
    double _gain = 1.0;
    double _from = 1.0;
    double _target = 1.0;
    double _elapsed = 0.0;
    double _duration = 0.0;
};
}
