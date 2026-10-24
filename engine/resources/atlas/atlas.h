#pragma once
#include <SDL3/SDL.h>

#include "../../core/geometry/rect.h"

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace elysia::resources
{
struct FrameInfo
{
	std::filesystem::path _path;
	SDL_Texture* _texture = nullptr;
	SDL_Texture* _coverage_mask = nullptr;
	int _width = 0;
	int _height = 0;
	size_t _index = 0;
	std::optional<elysia::core::Rect> _source_rect;
};

struct AtlasFrameTextures
{
	SDL_Texture* texture = nullptr;
	SDL_Texture* coverage_mask = nullptr;
};

class Atlas
{
public:
	Atlas() = default;
	explicit Atlas(std::string name);
	~Atlas() = default;

	void clear();
	void set_name(std::string name);

	const std::string& name() const;
	bool empty() const;
	size_t size() const;

	bool add_frame(
		const std::filesystem::path& frame_path,
		SDL_Texture* texture,
		SDL_Texture* coverage_mask,
		std::optional<elysia::core::Rect> source_rect = std::nullopt
	);
	bool add_texture(SDL_Texture* texture,SDL_Texture* coverage_mask);
	bool add_textures(std::initializer_list<AtlasFrameTextures> textures);

	const FrameInfo* frame_at(size_t frame_index) const;
	SDL_Texture* texture_at(size_t frame_index) const;

private:
	std::string _name;
	std::vector<FrameInfo> _frames;
};

}
