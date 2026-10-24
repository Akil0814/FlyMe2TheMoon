#include "resource_service.h"

#include "runtime/resource_manager.h"

namespace elysia::resources
{
const Atlas* ResourceService::find_atlas(std::string_view key) const
{
	return ResourceManager::instance()->find_atlas(key);
}

bool ResourceService::has_font(std::string_view key) const noexcept
{
	return ResourceManager::instance()->has_font(key);
}

TTF_Font* ResourceService::find_font(std::string_view key) const
{
	return ResourceManager::instance()->find_font(key);
}

MIX_Audio* ResourceService::find_sound(std::string_view key) const
{
	return ResourceManager::instance()->find_sound(key);
}

MIX_Audio* ResourceService::find_music(std::string_view key) const
{
	return ResourceManager::instance()->find_music(key);
}

SDL_Texture* ResourceService::find_texture(std::string_view key) const
{
	return ResourceManager::instance()->find_texture(key);
}
}
