#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"

#include <string_view>

namespace elysia::io
{
enum class PathError
{
    ProjectRoot,
    FilesystemAccess,
    RequiredAssetDirectory,
    RuntimeDirectory
};

struct PathFailure
{
    PathError code = PathError::ProjectRoot;
    elysia::core::FailureDiagnostic diagnostic;

    [[nodiscard]] std::string_view error_code() const noexcept
    {
        switch (code)
        {
        case PathError::ProjectRoot: return "BOOTSTRAP-PROJECT-ROOT";
        case PathError::FilesystemAccess: return "BOOTSTRAP-PATH-ACCESS";
        case PathError::RequiredAssetDirectory: return "BOOTSTRAP-ASSET-DIRECTORY";
        case PathError::RuntimeDirectory: return "BOOTSTRAP-RUNTIME-DIRECTORY";
        }
        return "BOOTSTRAP-PATH";
    }
};
}
