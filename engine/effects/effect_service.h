#pragma once

#include "effect_types.h"
#include "screen/screen_effect_types.h"
#include "../tools/singleton.h"

#define ELYSIA_EFFECTS (::elysia::effects::EffectService::instance())

namespace elysia::effects
{
class EffectService final : public elysia::tools::Singleton<EffectService>
{
	friend elysia::tools::Singleton<EffectService>;

public:
	// Screen effects belong to the active scene and never survive its exit.
	[[nodiscard]] std::optional<ScreenEffectHandle> request_screen_color_effect(const ScreenColorEffectRequest& request);
	[[nodiscard]] std::optional<ScreenEffectHandle> request_screen_image_effect(const ScreenImageEffectRequest& request);
	[[nodiscard]] std::optional<ScreenEffectHandle> request_screen_animation_effect(const ScreenAnimationEffectRequest& request);
	// Stops from current opacity using the configured fade-out duration.
	bool stop_screen_effect(ScreenEffectHandle handle) noexcept;
	bool cancel_screen_effect(ScreenEffectHandle handle) noexcept;
	[[nodiscard]] bool is_screen_effect_active(ScreenEffectHandle handle) const noexcept;
	[[nodiscard]] bool request_animation_effect(
		const AnimationEffectSpawnRequest& request);
	[[nodiscard]] bool request_floating_number_effect(
		const FloatingNumberEffectSpawnRequest& request);

private:
	EffectService() = default;
};
}
