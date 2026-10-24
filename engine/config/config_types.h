#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

#include <string>
#include <source_location>

namespace elysia::config
{
struct ConfigOrigin
{
    std::string config_path;
    std::string json_pointer;
    std::string key_namespace;
    std::string full_key;

    [[nodiscard]] std::string describe() const;
};

enum class ConfigLoadError
{
    OpenFailed,
    FileMissing,
    FilesystemAccess,
    UnavailableDependency,
    InvalidSchema,
    InvalidKey,
    InvalidValue,
    DuplicateKey
};
struct ConfigLoadFailure
{
    ConfigLoadError error = ConfigLoadError::InvalidSchema;
    std::string message;
    ConfigOrigin first;
    ConfigOrigin second;
    std::source_location origin = std::source_location::current();

    [[nodiscard]] elysia::core::FailureDiagnostic diagnostic() const;
};

enum class ConfigAccessError { NotInitialized, MissingKey, TypeMismatch, InvalidValue };
struct ConfigAccessFailure
{
    ConfigAccessError error = ConfigAccessError::MissingKey;
    std::string key;
    std::string expected_type;
    std::string actual_type;
    ConfigOrigin origin;
    std::string message;
};
}
