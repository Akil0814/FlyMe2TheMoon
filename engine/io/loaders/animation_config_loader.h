#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"
#include "../json/json_loader.h"

#include <expected>
#include <cstddef>
#include <filesystem>
#include <string>

namespace elysia::io
{
class AnimationConfigLoader
{
public:
	[[nodiscard]] std::expected<AnimationConfig,ManifestLoadFailure> load(
		const std::filesystem::path& animation_config_path,
		const AnimationLayout& layout
	) const;

private:
	[[nodiscard]] std::expected<std::filesystem::path,ManifestLoadFailure> resolve_clip_path(
		const std::filesystem::path& config_path,
		const std::string& json_pointer,
		const std::string& animation_name,
		bool is_segment,
		size_t segment_index,
		const AnimationLayout& layout
	) const;

	[[nodiscard]] std::expected<void,ManifestLoadFailure> append_clip(
		const std::filesystem::path& config_path,
		const std::string& json_pointer,
		const std::string& animation_name,
		bool is_segment,
		size_t segment_index,
		const json& clip_node,
		const AnimationLayout& layout,
		AnimationConfig& config
	) const;
};
}
