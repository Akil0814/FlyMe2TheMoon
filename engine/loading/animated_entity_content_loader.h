#pragma once

#include "../io/loaders/asset_config_types.h"
#include "content_load_failure.h"

#include <expected>
#include <filesystem>
#include <string>

namespace elysia::loading
{
class AnimatedEntityContentLoader
{
public:
	[[nodiscard]] std::expected<elysia::io::EntityContentModule,ContentLoadFailure> load(
		const std::string& module_name,
		const std::filesystem::path& module_manifest_path
	) const;
};
}
