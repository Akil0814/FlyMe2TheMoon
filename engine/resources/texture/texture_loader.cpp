#include "texture_loader.h"
namespace elysia::resources
{
void TextureDeleter::operator()(SDL_Texture* texture) const
{
	if (texture)
		SDL_DestroyTexture(texture);
}

TexturePtr TextureLoader::create_texture(
	SDL_Renderer* renderer,
	const SDL_Surface& surface) const
{
	if (!renderer)
		return {};

	return TexturePtr(SDL_CreateTextureFromSurface(
		renderer,
		const_cast<SDL_Surface*>(&surface)));
}

std::expected<TextureLoadResult,ResourceFailure> TextureLoader::load_texture(
	SDL_Renderer* renderer,
	const SurfaceLoadResult& surface_result
) const
{
	TextureLoadResult result;
	result._asset_key = surface_result._asset_key;
	result._frame_path = surface_result._frame_path;
	result._frame_index = surface_result._frame_index;

	if (!renderer)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load texture failed: renderer is null.",
			surface_result._subject_type,surface_result._asset_key,
			surface_result._frame_path,surface_result._origin));

	if (!surface_result._surface)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load texture failed: surface is invalid.",
			surface_result._subject_type,surface_result._asset_key,
			surface_result._frame_path,surface_result._origin));

	TexturePtr texture = create_texture(renderer,*surface_result._surface);
	if (!texture)
		return std::unexpected(make_resource_failure(
			ResourceError::CreateFailed,
			std::string("Load texture failed: ") + SDL_GetError(),
			surface_result._subject_type,surface_result._asset_key,
			surface_result._frame_path,surface_result._origin));

	result._texture = std::move(texture);
	return result;
}


}
