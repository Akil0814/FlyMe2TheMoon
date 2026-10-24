#pragma once

#include "../animation/animation_effect_factory.h"
#include "../effect_types.h"
#include "../screen/screen_effect_runtime.h"
#include "../number/floating_number_effect_factory.h"
#include "../../resources/resource_types.h"
#include "../../tools/singleton.h"
#include "../effect_registration_failure.h"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::scene
{
class Scene;
class SceneManager;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::effects
{
class EffectService;

class EffectManager : public elysia::tools::Singleton<EffectManager>
{
	friend elysia::tools::Singleton<EffectManager>;
	friend class EffectService;
	friend class elysia::scene::SceneManager;

public:
	void clear_screen_effects() noexcept { _screen_effects.clear(); }
	void append_screen_effect_commands(ScreenEffectLayer layer, const elysia::core::Rect& viewport,
		std::vector<elysia::core::UiRenderCommand>& out) const { _screen_effects.append_commands(layer, viewport, out); }
	[[nodiscard]] elysia::core::RenderResult render_screen_effects(SDL_Renderer* renderer, ScreenEffectLayer layer,
		const elysia::core::Rect& viewport) const { return _screen_effects.render(renderer, layer, viewport); }
	void set_runtime_dependencies(
		SDL_Renderer* renderer,
		const elysia::typography::FontResolver* font_resolver) noexcept;

	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const elysia::resources::AnimationEffectBuildRequest& request);
	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const std::vector<elysia::resources::AnimationEffectBuildRequest>& requests);

	[[nodiscard]] const AnimationEffectDefinition* find_animation_effect_definition(
		std::string_view key) const;
	void clear_content() noexcept;

private:
	std::optional<ScreenEffectHandle> dispatch(const ScreenColorEffectRequest& request);
	std::optional<ScreenEffectHandle> dispatch(const ScreenImageEffectRequest& request);
	std::optional<ScreenEffectHandle> dispatch(const ScreenAnimationEffectRequest& request);
	ScreenEffectRuntime _screen_effects;
	[[nodiscard]] bool dispatch(const AnimationEffectSpawnRequest& request);
	[[nodiscard]] bool dispatch(const FloatingNumberEffectSpawnRequest& request);

	void bind_active_scene(elysia::scene::Scene& scene) noexcept;
	void unbind_active_scene(const elysia::scene::Scene& scene) noexcept;

	std::unordered_map<std::string,AnimationEffectDefinition> _animation_effect_definitions;
	AnimationEffectFactory _animation_effect_factory;
	FloatingNumberEffectFactory _floating_number_effect_factory;
	elysia::scene::Scene* _active_scene = nullptr;
};
}
