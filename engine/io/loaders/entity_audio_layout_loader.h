#pragma once
#include "asset_config_types.h"
#include "manifest_load_failure.h"

#include <expected>
#include <filesystem>

namespace elysia::io
{
class EntityAudioLayoutLoader
{
public:
    [[nodiscard]] std::expected<EntityAudioLayout,ManifestLoadFailure> load(
        const std::filesystem::path&) const;
};
}
