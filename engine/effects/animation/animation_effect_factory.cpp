#include "animation_effect_factory.h"

#include "../../animation/animation_service.h"
#include "../../tools/logger.h"

#include <algorithm>

namespace elysia::effects
{
namespace
{
std::optional<elysia::core::Vector2> resolve_effect_size(
	const AnimationEffectSpawnRequest& request,
	const AnimationEffectDefinition& definition,
	const elysia::animation::Animation& animation)
{
	if (request.size.has_value())
		return request.size;

	if (!definition.default_size.is_zero())
		return definition.default_size;

	const elysia::resources::FrameInfo* frame = animation.current_frame();
	if (!frame || frame->_width <= 0 || frame->_height <= 0)
		return std::nullopt;

	return elysia::core::Vector2(
		static_cast<float>(frame->_width),
		static_cast<float>(frame->_height));
}

elysia::core::Vector2 get_effect_top_left(
	const elysia::core::Vector2& anchor_position,
	const elysia::core::Vector2& size,
	EffectAnchor anchor)
{
	switch (anchor)
	{
	case EffectAnchor::TopLeft:
		return anchor_position;
	case EffectAnchor::TopCenter:
		return {anchor_position.x - size.x * 0.5f,anchor_position.y};
	case EffectAnchor::TopRight:
		return {anchor_position.x - size.x,anchor_position.y};
	case EffectAnchor::CenterLeft:
		return {anchor_position.x,anchor_position.y - size.y * 0.5f};
	case EffectAnchor::Center:
		return {anchor_position.x - size.x * 0.5f,anchor_position.y - size.y * 0.5f};
	case EffectAnchor::CenterRight:
		return {anchor_position.x - size.x,anchor_position.y - size.y * 0.5f};
	case EffectAnchor::BottomLeft:
		return {anchor_position.x,anchor_position.y - size.y};
	case EffectAnchor::BottomCenter:
		return {anchor_position.x - size.x * 0.5f,anchor_position.y - size.y};
	case EffectAnchor::BottomRight:
		return {anchor_position.x - size.x,anchor_position.y - size.y};
	default:
		return anchor_position;
	}
}
}

std::unique_ptr<AnimationEffect> AnimationEffectFactory::create(
	const AnimationEffectSpawnRequest& request,
	const AnimationEffectDefinition& definition) const
{
	std::unique_ptr<elysia::animation::Animation> animation =
		ELYSIA_ANIMATIONS->create_animation(definition.animation_key);
	if (!animation)
	{
		ELYSIA_LOG_WARN("effects","Create effect failed: animation creation failed: "
			<< definition.animation_key);
		return nullptr;
	}

	const std::optional<elysia::core::Vector2> final_size =
		resolve_effect_size(request,definition,*animation);
	std::unique_ptr<AnimationEffect> effect = std::make_unique<AnimationEffect>(
		definition.effect_key,definition.animation_key,std::move(animation));

	if (final_size.has_value())
		effect->set_size(*final_size);

	effect->set_position(get_effect_top_left(request.position,effect->size(),request.anchor));
	effect->set_angle(request.angle_degrees.value_or(definition.angle_degrees));
	effect->set_flip(request.flip.value_or(elysia::core::SpriteFlip::None));
	effect->set_start_delay(std::max(0.0,request.start_delay_seconds));
	effect->set_on_started(request.on_started);
	effect->set_on_finished(request.on_finished);
	for (const AnimationEffectSpawnRequest::ScheduledCallbackRequest& callback : request.scheduled_callbacks)
		effect->schedule_callback(callback.delay_seconds,callback.callback);

	return effect;
}
}
