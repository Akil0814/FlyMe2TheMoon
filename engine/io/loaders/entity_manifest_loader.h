#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class EntityManifestLoader
{
public:
	[[nodiscard]] std::expected<EntityManifest,ManifestLoadFailure> load(
		const std::filesystem::path& manifest_path) const;
};
}
