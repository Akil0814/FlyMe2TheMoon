#include "effect_manager.h"

#include "../../animation/animation_service.h"
#include "../../scene/scene.h"
#include "../../resources/resource_service.h"
#include "../../core/time.h"
#include "../../tools/logger.h"

namespace elysia::effects
{
std::optional<ScreenEffectHandle> EffectManager::dispatch(const ScreenColorEffectRequest& request)
{
    auto handle = _screen_effects.create(request, _active_scene, elysia::core::Time::instance()->frame_count());
    if (!handle) ELYSIA_LOG_WARN("effects", "Screen color effect rejected: no active scene or invalid playback parameters.");
    return handle;
}
std::optional<ScreenEffectHandle> EffectManager::dispatch(const ScreenImageEffectRequest& request)
{
    auto handle = _screen_effects.create(request, ELYSIA_RESOURCES->find_texture(request.texture_key), _active_scene,
        elysia::core::Time::instance()->frame_count());
    if (!handle) ELYSIA_LOG_WARN("effects", "Screen image effect rejected: no active scene, invalid parameters or missing texture: " << request.texture_key);
    return handle;
}
std::optional<ScreenEffectHandle> EffectManager::dispatch(const ScreenAnimationEffectRequest& request)
{
    const auto* effect = find_animation_effect_definition(request.effect_key);
    const auto* definition = effect ? ELYSIA_ANIMATIONS->find_definition(effect->animation_key) : nullptr;
    if (!_active_scene || !definition)
    {
        ELYSIA_LOG_WARN("effects", "Screen animation effect rejected: no active scene or missing effect/animation: " << request.effect_key);
        return {};
    }
    auto animation = ELYSIA_ANIMATIONS->create_animation(effect->animation_key);
    auto handle = _screen_effects.create(request, std::move(animation), *definition,
        effect->default_size, request.angle_degrees.value_or(effect->angle_degrees), _active_scene,
        elysia::core::Time::instance()->frame_count());
    if (!handle) ELYSIA_LOG_WARN("effects", "Screen animation effect rejected: invalid playback, layout or atlas frames: " << request.effect_key);
    return handle;
}
void EffectManager::set_runtime_dependencies(
	SDL_Renderer* renderer,
	const elysia::typography::FontResolver* font_resolver) noexcept
{
	_floating_number_effect_factory.set_runtime_dependencies(renderer,font_resolver);
}

std::expected<void,EffectRegistrationFailure> EffectManager::register_animation_effect(
	const std::vector<elysia::resources::AnimationEffectBuildRequest>& requests)
{
	for (const elysia::resources::AnimationEffectBuildRequest& request : requests)
	{
		if (auto result = register_animation_effect(request); !result)
			return result;
	}

	return {};
}

std::expected<void,EffectRegistrationFailure> EffectManager::register_animation_effect(
	const elysia::resources::AnimationEffectBuildRequest& request)
{
	if (request.effect_key.empty())
	{
		return std::unexpected(EffectRegistrationFailure{
			EffectRegistrationError::InvalidKey,
			elysia::core::make_failure_diagnostic("Register effect failed: effect key is empty.")});
	}

	if (request.animation_key.empty())
	{
		return std::unexpected(EffectRegistrationFailure{
			EffectRegistrationError::InvalidAnimationKey,
			elysia::core::make_failure_diagnostic("Register effect failed: animation key is empty.",request.effect_key)});
	}

	if (request.default_size.x < 0.0f || request.default_size.y < 0.0f
		|| ((request.default_size.x == 0.0f) != (request.default_size.y == 0.0f)))
	{
		return std::unexpected(EffectRegistrationFailure{
			EffectRegistrationError::InvalidSize,
			elysia::core::make_failure_diagnostic("Register effect failed: default size must provide positive width and height.",request.effect_key)});
	}

	if (!ELYSIA_ANIMATIONS->find_definition(request.animation_key))
	{
		return std::unexpected(EffectRegistrationFailure{
			EffectRegistrationError::MissingAnimation,
			elysia::core::make_failure_diagnostic("Register effect failed: can't find animation definition.",request.effect_key,request.origin.config_path)});
	}

	AnimationEffectDefinition definition;
	definition.effect_key = request.effect_key;
	definition.animation_key = request.animation_key;
	definition.default_size = request.default_size;
	definition.angle_degrees = request.default_angle_degrees;
	_animation_effect_definitions[request.effect_key] = std::move(definition);
	return {};
}

const AnimationEffectDefinition* EffectManager::find_animation_effect_definition(
	std::string_view key) const
{
	const auto iterator = _animation_effect_definitions.find(std::string(key));
	if (iterator == _animation_effect_definitions.end())
		return nullptr;

	return &iterator->second;
}

bool EffectManager::dispatch(const AnimationEffectSpawnRequest& request)
{
	if (!_active_scene)
	{
		ELYSIA_LOG_WARN("effects","Spawn animation effect failed: there is no active scene.");
		return false;
	}

	const AnimationEffectDefinition* definition =
		find_animation_effect_definition(request.effect_key);
	if (!definition)
	{
		ELYSIA_LOG_WARN("effects","Create effect failed: definition does not exist: "
			<< request.effect_key);
		return false;
	}

	std::unique_ptr<AnimationEffect> effect =
		_animation_effect_factory.create(request,*definition);
	if (!effect)
		return false;

	if (_active_scene->add_object(std::move(effect)))
		return true;

	ELYSIA_LOG_WARN("effects","Spawn animation effect failed: active scene rejected the effect.");
	return false;
}

bool EffectManager::dispatch(const FloatingNumberEffectSpawnRequest& request)
{
	if (!_active_scene)
	{
		ELYSIA_LOG_WARN("effects","Spawn floating number effect failed: there is no active scene.");
		return false;
	}

	std::unique_ptr<FloatingNumberEffect> effect =
		_floating_number_effect_factory.create(request);
	if (!effect)
		return false;

	if (_active_scene->add_object(std::move(effect)))
		return true;

	ELYSIA_LOG_WARN("effects","Spawn floating number effect failed: active scene rejected the effect.");
	return false;
}

void EffectManager::clear_content() noexcept
{
	_screen_effects.clear();
	_animation_effect_definitions.clear();
	_floating_number_effect_factory.clear_cache();
}

void EffectManager::bind_active_scene(elysia::scene::Scene& scene) noexcept
{
	_active_scene = &scene;
}

void EffectManager::unbind_active_scene(const elysia::scene::Scene& scene) noexcept
{
	_screen_effects.unbind(&scene);
	if (_active_scene == &scene)
		_active_scene = nullptr;
}
}
