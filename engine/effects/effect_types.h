#pragma once

#include "animation/animation_effect.h"
#include "effect_anchor.h"
#include "number/floating_number_effect.h"

#include <optional>
#include <string>
#include <vector>

namespace elysia::effects
{
struct AnimationEffectDefinition
{
	std::string effect_key;
	std::string animation_key;
	double angle_degrees = 0.0;
	elysia::core::Vector2 default_size;
};

struct AnimationEffectSpawnRequest
{
	struct ScheduledCallbackRequest
	{
		double delay_seconds = 0.0;
		AnimationEffect::Callback callback;
	};

	std::string effect_key;
	// World-space position of the selected playback anchor.
	elysia::core::Vector2 position;
	EffectAnchor anchor = EffectAnchor::TopLeft;
	std::optional<elysia::core::Vector2> size;
	std::optional<double> angle_degrees;
	std::optional<elysia::core::SpriteFlip> flip;
	double start_delay_seconds = 0.0;
	AnimationEffect::Callback on_started;
	AnimationEffect::Callback on_finished;
	std::vector<ScheduledCallbackRequest> scheduled_callbacks;
};

struct FloatingNumberEffectSpawnRequest
{
	std::string text;
	FloatingNumberColor color = FloatingNumberColor::White;
	elysia::core::Vector2 position;
	FloatingNumberAlignment alignment = FloatingNumberAlignment::Center;
	float target_height = 20.0f;
	double start_delay_seconds = 0.0;
	double time_scale = 1.0;
	double lifetime_seconds = 0.6;
	FloatingNumberEffects effects;
	FloatingNumberEffect::Callback on_finished;
};
}
