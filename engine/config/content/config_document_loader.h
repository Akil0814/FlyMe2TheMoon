#pragma once

#include "config_loading_types.h"

#include <expected>

namespace elysia::config
{
class ConfigDocumentLoader
{
public:
    [[nodiscard]] std::expected<ConfigDocument,ConfigLoadFailure> load(
        const ConfigManifestEntry& entry) const;
};
}
