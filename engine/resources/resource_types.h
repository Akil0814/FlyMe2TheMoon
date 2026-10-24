#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "../core/geometry/vector2.h"
#include "resource_origin.h"

namespace elysia::resources
{
enum class AtlasSourceType
{
	FrameDirectory,
	HorizontalStrip
};

struct TextureLoadRequest
{
	std::string key;
	std::filesystem::path file_path;
	ResourceOrigin origin;
};

struct FontLoadRequest
{
	std::string key;
	std::filesystem::path file_path;
	int point_size = 0;
	ResourceOrigin origin;
};

struct SoundLoadRequest
{
	std::string key;
	std::filesystem::path file_path;
	ResourceOrigin origin;
};

struct MusicLoadRequest
{
	std::string key;
	std::filesystem::path file_path;
	ResourceOrigin origin;
};

struct AtlasBuildRequest
{
	[[nodiscard]] bool is_valid() const
	{
		if (atlas_key.empty() || source_path.empty() || frame_count == 0) return false;
		if (source_type == AtlasSourceType::FrameDirectory)
			return !frame_filename_prefix.empty();
		return frame_filename_prefix.empty();
	}

	std::string atlas_key;
	std::filesystem::path source_path;
	size_t frame_count = 0;
	std::string frame_filename_prefix;
	AtlasSourceType source_type = AtlasSourceType::FrameDirectory;
	ResourceOrigin origin;
};

struct AnimationBuildRequest
{
	std::string animation_key;
	std::string atlas_key;
	double fps = 10.0;
	bool loop = true;
	size_t segment_index = 0;
	ResourceOrigin origin;
};

struct AnimationEffectBuildRequest
{
	std::string effect_key;
	std::string animation_key;
	elysia::core::Vector2 default_size;
	double default_angle_degrees = 0.0;
	ResourceOrigin origin;
};

}
