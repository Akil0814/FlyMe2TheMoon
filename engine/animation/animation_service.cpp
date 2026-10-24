#include "animation_service.h"

#include "runtime/animation_manager.h"
#include "../tools/logger.h"

namespace elysia::animation
{
const AnimationDefinition* AnimationService::find_definition(std::string_view key) const
{
	return AnimationManager::instance()->find_definition(key);
}

std::unique_ptr<Animation> AnimationService::create_animation(std::string_view key) const
{
	const AnimationDefinition* definition = find_definition(key);
	if (!definition)
	{
		ELYSIA_LOG_WARN("animation","Create animation failed: definition does not exist: " << key);
		return nullptr;
	}

	std::unique_ptr<Animation> animation = std::make_unique<Animation>();
	animation->set_atlas(definition->atlas);
	animation->set_loop(definition->loop);
	animation->set_interval_seconds(1.0 / definition->fps);
	return animation;
}
}
