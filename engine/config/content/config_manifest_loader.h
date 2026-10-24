#pragma once

#include "config_loading_types.h"

#include <expected>
#include <filesystem>

namespace elysia::config
{
class ConfigManifestLoader
{
public:
    [[nodiscard]] std::expected<ConfigManifest,ConfigLoadFailure> load(
        const std::filesystem::path& path) const;
};
}
