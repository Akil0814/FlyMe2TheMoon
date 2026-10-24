#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class AnimationManifestLoader
{
public:
	[[nodiscard]] std::expected<AnimationManifest,ManifestLoadFailure> load(
		const std::filesystem::path& manifest_path) const;
};
}
