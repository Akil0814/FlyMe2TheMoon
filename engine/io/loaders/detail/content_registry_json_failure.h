#pragma once

#include "../content_registry_failure.h"
#include "../../json/strict_json.h"

namespace elysia::io::detail
{
inline ContentRegistryFailure registry_failure_from_json(const JsonFileFailure& failure)
{
    ContentRegistryError code = ContentRegistryError::InvalidDocument;
    switch (failure.code)
    {
    case JsonFileError::FileMissing: code = ContentRegistryError::FileMissing; break;
    case JsonFileError::FilesystemAccess:
    case JsonFileError::ReadFailed: code = ContentRegistryError::FilesystemAccess; break;
    case JsonFileError::EmptyPath:
    case JsonFileError::OpenFailed:
    case JsonFileError::ParseFailed:
    case JsonFileError::DuplicateProperty: break;
    }
    return {code,core::make_failure_diagnostic(failure.message,
        {core::make_failure_diagnostic_entry("content-registry",{},failure.file_path,
            failure.file_path,failure.json_pointer,failure.message,failure.origin)},failure.origin)};
}
}
