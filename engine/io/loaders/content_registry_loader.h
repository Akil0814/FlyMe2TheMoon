#pragma once

#include "asset_config_types.h"
#include "content_registry_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class ContentRegistryLoader
{
public:
    [[nodiscard]] std::expected<ContentRegistry,ContentRegistryFailure> load(
        const std::filesystem::path& content_registry_path) const;
};
}
