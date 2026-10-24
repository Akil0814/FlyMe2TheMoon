#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"
#include "../json/strict_json.h"

#include <expected>
#include <system_error>

namespace elysia::io
{
enum class ManifestLoadError
{
    InvalidDocument,
    OpenFailed,
    FileMissing,
    InvalidSchema,
    InvalidField,
    MissingField,
    UnknownField,
    InvalidValue,
    DuplicateKey,
    InvalidResourceKey,
    MissingContent,
    FilesystemAccess,
    UnavailableDependency
};

struct ManifestLoadFailure
{
    ManifestLoadError code = ManifestLoadError::InvalidDocument;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ManifestLoadFailure make_manifest_load_failure(
    ManifestLoadError code,
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return ManifestLoadFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(resource_key),
            std::move(resource_path),origin)
    };
}

[[nodiscard]] inline ManifestLoadFailure make_manifest_load_failure(
    ManifestLoadError code,
    std::string message,
    std::string subject_type,
    std::string subject_key,
    std::filesystem::path expected_path,
    std::filesystem::path declaration_path,
    std::string declaration_pointer,
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    return ManifestLoadFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),
            {elysia::core::make_failure_diagnostic_entry(
                std::move(subject_type),std::move(subject_key),
                std::move(expected_path),std::move(declaration_path),
                std::move(declaration_pointer),reason,origin)},origin)
    };
}

[[nodiscard]] inline ManifestLoadFailure manifest_failure_from_json(
    const JsonFileFailure& failure,
    std::string subject_type,
    std::string message_prefix = {})
{
    ManifestLoadError code = ManifestLoadError::InvalidDocument;
    switch (failure.code)
    {
    case JsonFileError::EmptyPath:
    case JsonFileError::FileMissing: code = ManifestLoadError::FileMissing; break;
    case JsonFileError::ReadFailed:
    case JsonFileError::FilesystemAccess: code = ManifestLoadError::FilesystemAccess; break;
    case JsonFileError::OpenFailed: code = ManifestLoadError::OpenFailed; break;
    case JsonFileError::DuplicateProperty: code = ManifestLoadError::DuplicateKey; break;
    case JsonFileError::ParseFailed: code = ManifestLoadError::InvalidDocument; break;
    }
    return make_manifest_load_failure(
        code,message_prefix + failure.message,std::move(subject_type),
        failure.duplicate_property,failure.file_path,failure.file_path,
        failure.json_pointer,failure.origin);
}

[[nodiscard]] inline std::expected<void,ManifestLoadFailure>
validate_manifest_source(
    const std::filesystem::path& path,
    std::string subject_type,
    std::source_location origin = std::source_location::current())
{
    std::error_code error;
    if (std::filesystem::is_regular_file(path,error))
        return {};

    if (error && error != std::errc::no_such_file_or_directory)
        return std::unexpected(make_manifest_load_failure(
            ManifestLoadError::FilesystemAccess,
            "Manifest file access failed: " + error.message(),
            std::move(subject_type),{},path,path,{},origin));

    return std::unexpected(make_manifest_load_failure(
        ManifestLoadError::FileMissing,
        "Manifest file does not exist or is not a regular file: "
            + path.filename().string(),
        std::move(subject_type),{},path,path,{},origin));
}
}
