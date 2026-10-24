#include "time.h"

#include <algorithm>

namespace elysia::core
{
void Time::begin_frame(double raw_delta_seconds)
{
    _raw_delta_seconds = raw_delta_seconds > 0.0 ? raw_delta_seconds : 0.0;
    _scaled_delta_seconds = _raw_delta_seconds * _time_scale;
    _total_time_seconds += _raw_delta_seconds;
    _scaled_total_time_seconds += _scaled_delta_seconds;
    ++_frame_count;
}

void Time::reset()
{
    _raw_delta_seconds = 0.0;
    _scaled_delta_seconds = 0.0;
    _time_scale = 1.0;
    _total_time_seconds = 0.0;
    _scaled_total_time_seconds = 0.0;
    _frame_count = 0;
}

void Time::set_time_scale(double scale)
{
    _time_scale = std::max(0.0, scale);
}

double Time::raw_delta() const
{
    return _raw_delta_seconds;
}

double Time::delta() const
{
    return _scaled_delta_seconds;
}

double Time::time_scale() const
{
    return _time_scale;
}

double Time::total_time() const
{
    return _total_time_seconds;
}

double Time::scaled_total_time() const
{
    return _scaled_total_time_seconds;
}

std::size_t Time::frame_count() const
{
    return _frame_count;
}

}
