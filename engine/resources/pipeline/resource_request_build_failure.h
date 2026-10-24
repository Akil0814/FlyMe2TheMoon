#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"
#include "../resource_origin.h"

namespace elysia::resources
{
enum class ResourceRequestBuildError
{
    InvalidDeclaration,
    MissingSource,
    FilesystemAccess,
    UnavailableDependency
};
struct ResourceRequestBuildFailure
{
    ResourceRequestBuildError code = ResourceRequestBuildError::InvalidDeclaration;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ResourceRequestBuildFailure make_request_build_failure(
    ResourceRequestBuildError code,std::string message,std::string type = {},
    std::string key = {},std::filesystem::path expected_path = {},
    ResourceOrigin resource_origin = {},
    std::source_location origin = std::source_location::current())
{
    return ResourceRequestBuildFailure{
        code,
        elysia::core::make_failure_diagnostic(
            message,
            {elysia::core::make_failure_diagnostic_entry(
                std::move(type),std::move(key),std::move(expected_path),
                std::move(resource_origin.config_path),
                std::move(resource_origin.json_pointer),std::move(message),origin)},origin)
    };
}
}
