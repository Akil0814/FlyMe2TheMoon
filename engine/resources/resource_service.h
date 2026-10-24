#pragma once

#include "../tools/singleton.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string_view>

#define ELYSIA_RESOURCES (::elysia::resources::ResourceService::instance())

namespace elysia::resources
{
class Atlas;

class ResourceService final : public elysia::tools::Singleton<ResourceService>
{
	friend elysia::tools::Singleton<ResourceService>;

public:
	[[nodiscard]] const Atlas* find_atlas(std::string_view key) const;
	[[nodiscard]] bool has_font(std::string_view key) const noexcept;
	[[nodiscard]] TTF_Font* find_font(std::string_view key) const;
	[[nodiscard]] MIX_Audio* find_sound(std::string_view key) const;
	[[nodiscard]] MIX_Audio* find_music(std::string_view key) const;
	[[nodiscard]] SDL_Texture* find_texture(std::string_view key) const;

private:
	ResourceService() = default;
};
}
