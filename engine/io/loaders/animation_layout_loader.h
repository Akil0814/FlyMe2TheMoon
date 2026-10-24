#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"
#include <expected>
#include <filesystem>

namespace elysia::io
{
class AnimationLayoutLoader
{
public:
	[[nodiscard]] std::expected<AnimationLayout,ManifestLoadFailure> load(
		const std::filesystem::path& layout_path) const;
};
}
