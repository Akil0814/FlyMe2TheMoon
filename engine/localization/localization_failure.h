#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

#include <string_view>

namespace elysia::localization
{
enum class LocalizationError
{
    Dependency,
    Manifest,
    Locale,
    Language
};

struct LocalizationFailure
{
    LocalizationError code = LocalizationError::Locale;
    elysia::core::FailureDiagnostic diagnostic;

    [[nodiscard]] std::string_view error_code() const noexcept
    {
        switch (code)
        {
        case LocalizationError::Dependency: return "LOCALIZATION-DEPENDENCY";
        case LocalizationError::Manifest: return "LOCALIZATION-MANIFEST";
        case LocalizationError::Locale: return "LOCALIZATION-LOCALE";
        case LocalizationError::Language: return "LOCALIZATION-LANGUAGE";
        }
        return "LOCALIZATION-LOCALE";
    }
};

[[nodiscard]] inline LocalizationFailure make_localization_failure(
    LocalizationError code,std::string message,std::string type = {},
    std::string key = {},std::filesystem::path expected_path = {},
    std::filesystem::path declaration_path = {},std::string pointer = {},
    std::source_location origin = std::source_location::current())
{
    if (type.empty() && key.empty() && expected_path.empty()
        && declaration_path.empty() && pointer.empty())
        return LocalizationFailure{
            code,elysia::core::make_failure_diagnostic(std::move(message),{},{},origin)};
    const std::string reason = message;
    return LocalizationFailure{
        code,elysia::core::make_failure_diagnostic(
            std::move(message),{elysia::core::make_failure_diagnostic_entry(
                std::move(type),std::move(key),std::move(expected_path),
                std::move(declaration_path),std::move(pointer),reason,origin)},origin)};
}
}
