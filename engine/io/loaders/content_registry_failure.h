#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"

namespace elysia::io
{
enum class ContentRegistryError
{
    OpenFailed,
    FileMissing,
    InvalidDocument,
    InvalidSchema,
    MissingReferencedFile,
    FilesystemAccess,
    UnavailableDependency
};
struct ContentRegistryFailure
{
    ContentRegistryError code = ContentRegistryError::InvalidDocument;
    elysia::core::FailureDiagnostic diagnostic;
};
}
