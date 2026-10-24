#pragma once

#include "../../animation/animation.h"
#include "../../core/game_object.h"
#include "../../core/interface/updatable.h"

#include <memory>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace elysia::effects
{
class AnimationEffect : public elysia::core::GameObject, public elysia::core::Updatable
{
public:
	using Callback = std::function<void(AnimationEffect&)>;

	AnimationEffect(
		std::string effect_key,
		std::string animation_key,
		std::unique_ptr<elysia::animation::Animation> animation
	);
	~AnimationEffect() override = default;

	void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;
	void update(double delta) override;

	std::unique_ptr<AnimationEffect> clone() const;

	void set_angle(double angle_degrees);
	void set_flip(elysia::core::SpriteFlip flip);
	void set_start_delay(double delay_seconds);
	void set_on_started(Callback callback);
	void set_on_finished(Callback callback);
	void schedule_callback(double delay_seconds, Callback callback);

	[[nodiscard]] bool is_started() const noexcept;

private:
	struct ScheduledCallback
	{
		double execute_at_seconds = 0.0;
		std::size_t sequence = 0;
		Callback callback;
	};

	void start_playback();
	void invoke_due_callbacks();
	void cancel_scheduled_callbacks() noexcept;

	std::string _effect_key;
	std::string _animation_key;
	double _angle_degrees = 0.0;
	elysia::core::SpriteFlip _flip = elysia::core::SpriteFlip::None;
	std::unique_ptr<elysia::animation::Animation> _animation;
	double _start_delay_remaining_seconds = 0.0;
	double _playback_elapsed_seconds = 0.0;
	bool _started = false;
	bool _finished_callback_invoked = false;
	std::size_t _next_callback_sequence = 0;
	Callback _on_started;
	Callback _on_finished;
	std::vector<ScheduledCallback> _scheduled_callbacks;
};

}
