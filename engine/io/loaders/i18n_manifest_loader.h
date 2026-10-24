#pragma once

#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class I18nManifestLoader
{
public:
	[[nodiscard]] std::expected<I18nManifest,ManifestLoadFailure> load(
		const std::filesystem::path& manifest_path) const;
};

}
