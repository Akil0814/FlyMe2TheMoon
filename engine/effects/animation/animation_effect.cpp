#include "animation_effect.h"

#include "../../animation/animation_service.h"

#include <algorithm>

namespace elysia::effects
{
AnimationEffect::AnimationEffect(
    std::string effect_key,
    std::string animation_key,
    std::unique_ptr<elysia::animation::Animation> animation
)
    : elysia::core::GameObject(elysia::core::DepthLayer::EffectFront),
      _effect_key(std::move(effect_key)),
      _animation_key(std::move(animation_key)),
      _animation(std::move(animation))
{}

void AnimationEffect::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
	if (is_destroyed() || !_animation || _start_delay_remaining_seconds > 0.0)
		return;

	elysia::core::RenderCommand render_command;
	if (_animation->build_render_command(world_rect(), _angle_degrees, _flip, render_command))
		out_commands.push_back(render_command);
}

void AnimationEffect::update(double delta)
{
	if (!_animation || is_destroyed())
	{
		cancel_scheduled_callbacks();
		if (!_animation)
			elysia::core::GameObject::destroy();
		return;
	}

	double effect_delta = scaled_delta(delta);
	if (!_started)
	{
		if (_start_delay_remaining_seconds > effect_delta)
		{
			_start_delay_remaining_seconds -= effect_delta;
			return;
		}

		effect_delta -= _start_delay_remaining_seconds;
		_start_delay_remaining_seconds = 0.0;
		start_playback();
		if (is_destroyed())
		{
			cancel_scheduled_callbacks();
			return;
		}
	}

	_playback_elapsed_seconds += effect_delta;
	invoke_due_callbacks();
	if (is_destroyed())
	{
		cancel_scheduled_callbacks();
		return;
	}

	_animation->update(effect_delta);
	if (_animation->is_finished())
	{
		if (!_finished_callback_invoked && _on_finished)
		{
			_finished_callback_invoked = true;
			_on_finished(*this);
		}
		cancel_scheduled_callbacks();
		elysia::core::GameObject::destroy();
	}
}

std::unique_ptr<AnimationEffect> AnimationEffect::clone() const
{
    std::unique_ptr<elysia::animation::Animation> animation =
        ELYSIA_ANIMATIONS->create_animation(_animation_key);

	std::unique_ptr<AnimationEffect> effect = std::make_unique<AnimationEffect>(
        _effect_key,
        _animation_key,
        std::move(animation)
    );

    effect->set_position(position());
    effect->set_size(size());
	effect->set_angle(_angle_degrees);
	effect->set_flip(_flip);
	effect->set_start_delay(_start_delay_remaining_seconds);

    return effect;
}

void AnimationEffect::set_angle(double angle_degrees)
{
	_angle_degrees = angle_degrees;
}

void AnimationEffect::set_flip(elysia::core::SpriteFlip flip)
{
	_flip = flip;
}

void AnimationEffect::set_start_delay(double delay_seconds)
{
	if (_started)
		return;

	_start_delay_remaining_seconds = std::max(0.0, delay_seconds);
}

void AnimationEffect::set_on_started(Callback callback)
{
	_on_started = std::move(callback);
}

void AnimationEffect::set_on_finished(Callback callback)
{
	_on_finished = std::move(callback);
}

void AnimationEffect::schedule_callback(double delay_seconds, Callback callback)
{
	if (!callback || is_destroyed())
		return;

	ScheduledCallback scheduled;
	scheduled.execute_at_seconds = _playback_elapsed_seconds + std::max(0.0, delay_seconds);
	scheduled.sequence = _next_callback_sequence++;
	scheduled.callback = std::move(callback);
	_scheduled_callbacks.push_back(std::move(scheduled));
}

bool AnimationEffect::is_started() const noexcept
{
	return _started;
}

void AnimationEffect::start_playback()
{
	_started = true;
	if (_on_started)
		_on_started(*this);
}

void AnimationEffect::invoke_due_callbacks()
{
	while (!is_destroyed())
	{
		auto due = _scheduled_callbacks.end();
		for (auto iterator = _scheduled_callbacks.begin(); iterator != _scheduled_callbacks.end(); ++iterator)
		{
			if (iterator->execute_at_seconds > _playback_elapsed_seconds)
				continue;
			if (due == _scheduled_callbacks.end()
				|| iterator->execute_at_seconds < due->execute_at_seconds
				|| (iterator->execute_at_seconds == due->execute_at_seconds && iterator->sequence < due->sequence))
			{
				due = iterator;
			}
		}

		if (due == _scheduled_callbacks.end())
			return;

		Callback callback = std::move(due->callback);
		_scheduled_callbacks.erase(due);
		callback(*this);
	}
}

void AnimationEffect::cancel_scheduled_callbacks() noexcept
{
	_scheduled_callbacks.clear();
}

}
