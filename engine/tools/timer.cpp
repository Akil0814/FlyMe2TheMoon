#include "timer.h"

#include <algorithm>

namespace elysia::tools
{
void Timer::restart()
{
	_elapsed_seconds = 0.0;
	_has_shot = false;
	_is_paused = false;
	++_state_version;
}

void Timer::set_wait_time(double wait_time_seconds)
{
	_wait_time_seconds = wait_time_seconds > 0.0 ? wait_time_seconds : 0.0;
	++_state_version;
}

void Timer::set_one_shot(bool is_one_shot)
{
	_is_one_shot = is_one_shot;
	++_state_version;
}

void Timer::set_on_timeout(TimeoutCallback on_timeout)
{
	_on_timeout = std::move(on_timeout);
}

void Timer::pause()
{
	_is_paused = true;
	++_state_version;
}

void Timer::resume()
{
	_is_paused = false;
	++_state_version;
}

double Timer::get_wait_time() const noexcept
{
	return _wait_time_seconds;
}

double Timer::get_elapsed_time() const noexcept
{
	return is_finished() ? _wait_time_seconds : _elapsed_seconds;
}

double Timer::get_remaining_time() const noexcept
{
	if (_wait_time_seconds <= 0.0 || is_finished())
		return 0.0;

	return std::max(_wait_time_seconds - get_elapsed_time(), 0.0);
}

bool Timer::is_one_shot() const noexcept
{
	return _is_one_shot;
}

bool Timer::is_finished() const noexcept
{
	return _is_one_shot && _has_shot;
}

bool Timer::is_paused() const noexcept
{
	return _is_paused;
}

void Timer::update(double delta_seconds)
{
	if (_is_paused || _wait_time_seconds <= 0.0 || delta_seconds <= 0.0)
		return;

	_elapsed_seconds += delta_seconds;

	while (!_is_paused && _wait_time_seconds > 0.0 && _elapsed_seconds >= _wait_time_seconds)
	{
		const double current_wait_time_seconds = _wait_time_seconds;
		const bool current_is_one_shot = _is_one_shot;
		const unsigned long long version_before_callback = _state_version;

		if (current_is_one_shot)
		{
			if (_has_shot)
			{
				_elapsed_seconds = 0.0;
				_is_paused = true;
				break;
			}

			_elapsed_seconds = 0.0;
			_has_shot = true;
			_is_paused = true;
		}
		else
		{
			_elapsed_seconds -= current_wait_time_seconds;
		}

		if (_on_timeout)
		{
			_on_timeout();
		}

		if (_state_version != version_before_callback)
		{
			break;
		}

		if (current_is_one_shot)
		{
			break;
		}
	}
}

}
