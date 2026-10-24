#pragma once

#include "../animation_types.h"
#include "../../resources/resource_service.h"
#include "../../tools/singleton.h"
#include "../../resources/resource_types.h"
#include "../animation_registration_failure.h"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::animation
{
class AnimationService;

class AnimationManager : public elysia::tools::Singleton<AnimationManager>
{
	friend elysia::tools::Singleton<AnimationManager>;
	friend class AnimationService;

public:
	[[nodiscard]] std::expected<void,AnimationRegistrationFailure> register_animation(const elysia::resources::AnimationBuildRequest& request,const elysia::resources::Atlas* atlas);
	[[nodiscard]] std::expected<void,AnimationRegistrationFailure> register_animations(const std::vector<elysia::resources::AnimationBuildRequest>& requests,
		const elysia::resources::ResourceService& resource_service);

	void clear() noexcept;

private:
	AnimationManager() = default;

	[[nodiscard]] const AnimationDefinition* find_definition(std::string_view key) const;

	std::unordered_map<std::string, AnimationDefinition> _definitions;
};

}
