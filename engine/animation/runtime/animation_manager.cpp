#include "animation_manager.h"

#include "../../tools/logger.h"

#include "../../resources/resource_service.h"
namespace elysia::animation
{
std::expected<void,AnimationRegistrationFailure> AnimationManager::register_animation(
	const elysia::resources::AnimationBuildRequest& request,
	const elysia::resources::Atlas* atlas
)
{
	if (request.animation_key.empty())
	{
		return std::unexpected(AnimationRegistrationFailure{
			AnimationRegistrationError::InvalidKey,
			elysia::core::make_failure_diagnostic("Register animation failed: animation key is empty.")});
	}

	if (request.atlas_key.empty())
	{
		return std::unexpected(AnimationRegistrationFailure{
			AnimationRegistrationError::InvalidKey,
			elysia::core::make_failure_diagnostic("Register animation failed: atlas key is empty.",request.animation_key)});
	}

	if (!atlas)
	{
		return std::unexpected(AnimationRegistrationFailure{
			AnimationRegistrationError::MissingAtlas,
			elysia::core::make_failure_diagnostic("Register animation failed: atlas is null.",request.animation_key,request.origin.config_path)});
	}

	if (request.fps <= 0.0)
	{
		return std::unexpected(AnimationRegistrationFailure{
			AnimationRegistrationError::InvalidFps,
			elysia::core::make_failure_diagnostic("Register animation failed: fps is invalid.",request.animation_key)});
	}

	AnimationDefinition definition;
	definition.animation_key = request.animation_key;
	definition.atlas_key = request.atlas_key;
	definition.fps = request.fps;
	definition.loop = request.loop;
	definition.segment_index = request.segment_index;
	definition.atlas = atlas;

	_definitions[request.animation_key] = definition;
	return {};
}

std::expected<void,AnimationRegistrationFailure> AnimationManager::register_animations(const std::vector<elysia::resources::AnimationBuildRequest>& requests,
	const elysia::resources::ResourceService& resource_service)
{
	for (const elysia::resources::AnimationBuildRequest& request : requests)
	{
		const elysia::resources::Atlas* atlas = resource_service.find_atlas(request.atlas_key);
		if (auto result = register_animation(request, atlas); !result)
			return result;
	}

	return {};
}

const AnimationDefinition* AnimationManager::find_definition(std::string_view key) const
{
	std::unordered_map<std::string, AnimationDefinition>::const_iterator iterator =
		_definitions.find(std::string(key));
	if (iterator == _definitions.end())
	{
		ELYSIA_LOG_WARN("animation", "Find animation failed: definition does not exist: " << key);
		return nullptr;
	}

	return &iterator->second;
}

void AnimationManager::clear() noexcept
{
	_definitions.clear();
}

}
