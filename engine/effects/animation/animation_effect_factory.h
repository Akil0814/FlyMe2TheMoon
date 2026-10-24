#pragma once

#include "../effect_types.h"

#include <memory>

namespace elysia::effects
{
class AnimationEffectFactory
{
public:
	[[nodiscard]] std::unique_ptr<AnimationEffect> create(
		const AnimationEffectSpawnRequest& request,
		const AnimationEffectDefinition& definition) const;
};
}
