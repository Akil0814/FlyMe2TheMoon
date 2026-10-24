#pragma once

#include "localized_text_style.h"

#include <SDL3/SDL.h>

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace elysia::localization
{
struct SdlTextureDeleter
{
	void operator()(SDL_Texture* texture) const;
};

using CachedTexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;

struct TextTextureCacheKey
{
	std::string language;
	std::string translation_key;
	bool is_raw_text = false;
	elysia::typography::UiTypographyRole typography_role =
		elysia::typography::UiTypographyRole::Label;
	std::optional<elysia::typography::FontSource> font_source_override;
	std::uint64_t font_generation = 0;
	elysia::core::Color color{};
	int wrap_width = 0;

	bool operator==(const TextTextureCacheKey& other) const;
};

struct TextTextureCacheKeyHash
{
	size_t operator()(const TextTextureCacheKey& key) const;
};

class TextTextureCache
{
public:
	using TextureFactory = std::function<CachedTexturePtr()>;

	SDL_Texture* get_or_create(
		const std::string& language,
		std::uint64_t font_generation,
		std::string_view translation_key,
		const LocalizedTextStyle& style,
		const TextureFactory& texture_factory
	);
	SDL_Texture* get_or_create_raw(
		const std::string& language,
		std::uint64_t font_generation,
		std::string_view raw_text,
		const LocalizedTextStyle& style,
		const TextureFactory& texture_factory
	);

	void clear();

private:
	std::unordered_map<TextTextureCacheKey, CachedTexturePtr, TextTextureCacheKeyHash> _textures;
};

}
