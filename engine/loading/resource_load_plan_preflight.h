#pragma once

#include "content_load_failure.h"

#include <expected>
#include <vector>

namespace elysia::loading
{
class ResourceLoadPlan;
class ResourceLoadPlanPreflight
{
public:
    [[nodiscard]] std::expected<void,ContentLoadFailure> validate(
        const ResourceLoadPlan& plan,
        std::vector<elysia::core::FailureDiagnosticEntry> initial_missing = {}) const;
};
}
