#include "content_runtime_cleanup.h"

#include "../animation/runtime/animation_manager.h"
#include "../config/config_service.h"
#include "../effects/runtime/effect_manager.h"
#include "../resources/runtime/resource_manager.h"

namespace elysia::loading
{
void clear_loaded_content() noexcept
{
	elysia::effects::EffectManager::instance()->clear_content();
	elysia::animation::AnimationManager::instance()->clear();
	elysia::config::ConfigService::instance()->shutdown();
	elysia::resources::ResourceManager::instance()->clear();
}
}
