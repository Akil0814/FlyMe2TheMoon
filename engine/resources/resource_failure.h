#pragma once

#include "../core/diagnostics/failure_diagnostic.h"
#include "resource_origin.h"

namespace elysia::resources
{
enum class ResourceError
{
    InvalidRequest,
    MissingFile,
    DecodeFailed,
    CreateFailed,
    StoreFailed,
    DuplicateResource,
    MissingBuildState,
    InvalidBuildState
};

struct ResourceFailure
{
    ResourceError code = ResourceError::InvalidRequest;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ResourceFailure make_resource_failure(
    ResourceError code,
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return ResourceFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(resource_key),
            std::move(resource_path),origin)
    };
}

[[nodiscard]] inline ResourceFailure make_resource_failure(
    ResourceError code,
    std::string message,
    std::string subject_type,
    std::string resource_key,
    std::filesystem::path resource_path,
    ResourceOrigin resource_origin,
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    return ResourceFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),
            {elysia::core::make_failure_diagnostic_entry(
                std::move(subject_type),std::move(resource_key),
                std::move(resource_path),std::move(resource_origin.config_path),
                std::move(resource_origin.json_pointer),reason,origin)},origin)
    };
}
}
