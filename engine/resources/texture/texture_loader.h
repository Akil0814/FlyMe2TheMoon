#pragma once

#include "surface_loader.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <expected>
#include <memory>
#include <string>

namespace elysia::resources
{
struct TextureDeleter
{
	void operator()(SDL_Texture* texture) const;
};

using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;

struct TextureResource
{
	TexturePtr texture;
	TexturePtr coverage_mask;
};

struct TextureLoadResult
{
	std::string _asset_key;
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
	TexturePtr _texture;
};

class TextureLoader
{
public:
	[[nodiscard]] TexturePtr create_texture(
		SDL_Renderer* renderer,
		const SDL_Surface& surface) const;
	[[nodiscard]] std::expected<TextureLoadResult,ResourceFailure> load_texture(
		SDL_Renderer* renderer,
		const SurfaceLoadResult& surface_result
	) const;
};


}
