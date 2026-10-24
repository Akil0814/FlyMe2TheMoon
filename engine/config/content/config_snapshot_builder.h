#pragma once

#include "config_loading_types.h"
#include "config_snapshot.h"

#include <expected>
#include <memory>
#include <vector>

namespace elysia::config
{
class ConfigSnapshotBuilder
{
public:
    [[nodiscard]] std::expected<std::shared_ptr<const ConfigSnapshot>,ConfigLoadFailure> build(
        const std::vector<ConfigDocument>& documents) const;
};
}
