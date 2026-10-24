#pragma once
#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class EntityTextureLayoutLoader
{
public:
    [[nodiscard]] std::expected<EntityTextureLayout,ManifestLoadFailure> load(
        const std::filesystem::path&) const;
};
}
