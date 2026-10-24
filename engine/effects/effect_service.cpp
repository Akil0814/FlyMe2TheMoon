#include "effect_service.h"

#include "runtime/effect_manager.h"

namespace elysia::effects
{
std::optional<ScreenEffectHandle> EffectService::request_screen_color_effect(const ScreenColorEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
std::optional<ScreenEffectHandle> EffectService::request_screen_image_effect(const ScreenImageEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
bool EffectService::stop_screen_effect(ScreenEffectHandle handle) noexcept
{
    return EffectManager::instance()->_screen_effects.stop(handle);
}
std::optional<ScreenEffectHandle> EffectService::request_screen_animation_effect(const ScreenAnimationEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
bool EffectService::cancel_screen_effect(ScreenEffectHandle handle) noexcept
{
    return EffectManager::instance()->_screen_effects.cancel(handle);
}
bool EffectService::is_screen_effect_active(ScreenEffectHandle handle) const noexcept
{
    return EffectManager::instance()->_screen_effects.active(handle);
}
bool EffectService::request_animation_effect(
	const AnimationEffectSpawnRequest& request)
{
	return EffectManager::instance()->dispatch(request);
}

bool EffectService::request_floating_number_effect(
	const FloatingNumberEffectSpawnRequest& request)
{
	return EffectManager::instance()->dispatch(request);
}
}
