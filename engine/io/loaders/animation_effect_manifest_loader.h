#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class AnimationEffectManifestLoader
{
public:
	[[nodiscard]] std::expected<AnimationEffectManifest,ManifestLoadFailure> load(
		const std::filesystem::path& manifest_path) const;
};
}
