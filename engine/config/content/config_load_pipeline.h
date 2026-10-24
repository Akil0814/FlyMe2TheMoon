#pragma once

#include "config_snapshot.h"
#include "../config_types.h"

#include <expected>
#include <filesystem>
#include <memory>

namespace elysia::config
{
class ConfigLoadPipeline
{
public:
    [[nodiscard]] std::expected<std::shared_ptr<const ConfigSnapshot>,ConfigLoadFailure> load(
        const std::filesystem::path& manifest_path) const;
};
}
