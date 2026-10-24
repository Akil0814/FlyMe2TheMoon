#include "engine/core/render/sdl_texture_size.h"
#include "atlas.h"

namespace elysia::resources
{
static bool query_texture_size(SDL_Texture* texture, int& width, int& height)
{
	if (!texture)
		return false;

	return elysia::core::texture_pixel_size(texture,&width,&height);
}

Atlas::Atlas(std::string name)
	: _name(std::move(name))
{
}

void Atlas::clear()
{
	_frames.clear();
}

void Atlas::set_name(std::string name)
{
	_name = std::move(name);
}

const std::string& Atlas::name() const
{
	return _name;
}

bool Atlas::empty() const
{
	return _frames.empty();
}

size_t Atlas::size() const
{
	return _frames.size();
}

bool Atlas::add_frame(
	const std::filesystem::path& frame_path,
	SDL_Texture* texture,
	SDL_Texture* coverage_mask,
	std::optional<elysia::core::Rect> source_rect
)
{
	int width = 0;
	int height = 0;
	int mask_width = 0;
	int mask_height = 0;
	if (!query_texture_size(texture,width,height)
		|| !query_texture_size(coverage_mask,mask_width,mask_height)
		|| width != mask_width || height != mask_height)
		return false;
	if (source_rect.has_value())
	{
		const elysia::core::Rect texture_rect(
			0.0f,
			0.0f,
			static_cast<float>(width),
			static_cast<float>(height)
		);
		if (source_rect->width() <= 0.0f || source_rect->height() <= 0.0f
			|| !texture_rect.contains(*source_rect))
		{
			return false;
		}
	}

	FrameInfo frame_info;
	frame_info._path = frame_path;
	frame_info._texture = texture;
	frame_info._coverage_mask = coverage_mask;
	frame_info._width = source_rect.has_value()
		? static_cast<int>(source_rect->width())
		: width;
	frame_info._height = source_rect.has_value()
		? static_cast<int>(source_rect->height())
		: height;
	frame_info._index = _frames.size();
	frame_info._source_rect = source_rect;
	_frames.push_back(frame_info);
	return true;
}

bool Atlas::add_texture(SDL_Texture* texture,SDL_Texture* coverage_mask)
{
	return add_frame({},texture,coverage_mask);
}

bool Atlas::add_textures(std::initializer_list<AtlasFrameTextures> textures)
{
	if (textures.size() == 0)
		return false;

	for (const AtlasFrameTextures& texture : textures)
	{
		if (!add_texture(texture.texture,texture.coverage_mask))
			return false;
	}

	return true;
}

const FrameInfo* Atlas::frame_at(size_t frame_index) const
{
	if (_frames.empty())
		return nullptr;

	return &_frames[frame_index % _frames.size()];
}

SDL_Texture* Atlas::texture_at(size_t frame_index) const
{
	const FrameInfo* frame_info = frame_at(frame_index);
	return frame_info ? frame_info->_texture : nullptr;
}

}
