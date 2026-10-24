#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <expected>
#include <memory>
#include <string>

#include "../resource_failure.h"
#include "../resource_origin.h"

namespace elysia::resources
{
struct SurfaceDeleter
{
	void operator()(SDL_Surface* surface) const;
};

using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

struct SurfaceLoadRequest
{
	std::string _asset_key;
	std::string _subject_type = "texture";
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
	ResourceOrigin _origin;
};

struct SurfaceLoadResult
{
	std::string _asset_key;
	std::string _subject_type = "texture";
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
	ResourceOrigin _origin;
	SurfacePtr _surface;
};

class SurfaceLoader
{
public:
	[[nodiscard]] std::expected<SurfaceLoadResult,ResourceFailure>
	load_surface(const SurfaceLoadRequest& request) const;
};

[[nodiscard]] std::expected<SurfacePtr,ResourceFailure> create_coverage_mask_surface(
	const SDL_Surface& source_surface);

}
