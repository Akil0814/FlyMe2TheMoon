#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class EffectDefinitionConfigLoader
{
public:
	[[nodiscard]] std::expected<EffectDefinitionConfig,ManifestLoadFailure> load(
		const std::filesystem::path& config_path,
		const AnimationConfig& animation_config
	) const;
};
}
