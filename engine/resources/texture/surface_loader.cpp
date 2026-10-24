#include "surface_loader.h"

#include <SDL3_image/SDL_image.h>

#include <cstdint>
#include <cstring>
namespace elysia::resources
{
void SurfaceDeleter::operator()(SDL_Surface* surface) const
{
	if (surface)
		SDL_DestroySurface(surface);
}

std::expected<SurfaceLoadResult,ResourceFailure>
SurfaceLoader::load_surface(const SurfaceLoadRequest& request) const
{
	SurfaceLoadResult result;
	result._asset_key = request._asset_key;
	result._subject_type = request._subject_type;
	result._frame_path = request._frame_path;
	result._frame_index = request._frame_index;
	result._origin = request._origin;

	if (request._asset_key.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load surface failed: asset key is empty.",
			request._subject_type,{},request._frame_path,request._origin));

	if (request._frame_path.empty())
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,"Load surface failed: frame path is empty.",
			request._subject_type,request._asset_key,{},request._origin));

	SDL_Surface* surface = IMG_Load(request._frame_path.string().c_str());
	if (!surface)
		return std::unexpected(make_resource_failure(
			ResourceError::DecodeFailed,
			std::string("Load surface failed: ") + SDL_GetError(),
			request._subject_type,request._asset_key,request._frame_path,request._origin));

	result._surface.reset(surface);
	return result;
}

std::expected<SurfacePtr,ResourceFailure> create_coverage_mask_surface(
	const SDL_Surface& source_surface)
{
	if (source_surface.w <= 0 || source_surface.h <= 0
		|| !source_surface.format || !source_surface.pixels)
		return std::unexpected(make_resource_failure(
			ResourceError::InvalidRequest,
			"Create coverage mask failed: source surface is invalid."));

	SurfacePtr converted(SDL_ConvertSurface(const_cast<SDL_Surface*>(&source_surface), SDL_PIXELFORMAT_RGBA32));
	if (!converted)
		return std::unexpected(make_resource_failure(
			ResourceError::CreateFailed,
			std::string("Create coverage mask failed: convert source surface failed: ")
				+ SDL_GetError()));

	SurfacePtr mask(SDL_CreateSurface(converted->w, converted->h, SDL_PIXELFORMAT_RGBA32));
	if (!mask)
		return std::unexpected(make_resource_failure(
			ResourceError::CreateFailed,
			std::string("Create coverage mask failed: create surface failed: ")
				+ SDL_GetError()));

	const bool lock_converted = SDL_MUSTLOCK(converted.get()) != 0;
	const bool lock_mask = SDL_MUSTLOCK(mask.get()) != 0;
    const bool converted_locked = lock_converted && SDL_LockSurface(converted.get());
    const bool mask_locked = lock_mask && SDL_LockSurface(mask.get());
    if ((lock_converted && !converted_locked) || (lock_mask && !mask_locked))
    {
        if (converted_locked) SDL_UnlockSurface(converted.get());
        if (mask_locked) SDL_UnlockSurface(mask.get());
        return std::unexpected(make_resource_failure(ResourceError::CreateFailed,
            std::string("Create coverage mask failed: lock surface failed: ") + SDL_GetError()));
    }

	for (int y = 0; y < converted->h; ++y)
	{
		const auto* source_row = static_cast<const std::uint8_t*>(converted->pixels)
			+ static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch);
		auto* mask_row = static_cast<std::uint8_t*>(mask->pixels)
			+ static_cast<std::size_t>(y) * static_cast<std::size_t>(mask->pitch);
		for (int x = 0; x < converted->w; ++x)
		{
			std::uint32_t source_pixel = 0;
			std::memcpy(
				&source_pixel,
				source_row + static_cast<std::size_t>(x) * sizeof(std::uint32_t),
				sizeof(source_pixel));
			std::uint8_t red = 0;
			std::uint8_t green = 0;
			std::uint8_t blue = 0;
			std::uint8_t alpha = 0;
			SDL_GetRGBA(source_pixel,SDL_GetPixelFormatDetails(converted->format),nullptr,&red,&green,&blue,&alpha);
			const std::uint32_t mask_pixel =
				SDL_MapRGBA(SDL_GetPixelFormatDetails(mask->format),nullptr,255,255,255,alpha);
			std::memcpy(
				mask_row + static_cast<std::size_t>(x) * sizeof(std::uint32_t),
				&mask_pixel,
				sizeof(mask_pixel));
		}
	}

	if (lock_mask)
		SDL_UnlockSurface(mask.get());
	if (lock_converted)
		SDL_UnlockSurface(converted.get());
	return mask;
}


}
