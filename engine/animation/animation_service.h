#pragma once

#include "animation.h"
#include "animation_types.h"
#include "../tools/singleton.h"

#include <memory>
#include <string_view>

#define ELYSIA_ANIMATIONS (::elysia::animation::AnimationService::instance())

namespace elysia::animation
{
class AnimationService final : public elysia::tools::Singleton<AnimationService>
{
	friend elysia::tools::Singleton<AnimationService>;

public:
	[[nodiscard]] const AnimationDefinition* find_definition(std::string_view key) const;
	[[nodiscard]] std::unique_ptr<Animation> create_animation(std::string_view key) const;

private:
	AnimationService() = default;
};
}
