#pragma once

#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elysia::core
{
struct FailureDiagnosticEntry
{
    std::string subject_type;
    std::string subject_key;
    std::filesystem::path expected_path;
    std::filesystem::path declaration_path;
    std::string declaration_pointer;
    std::string reason;
    std::source_location origin = std::source_location::current();
};

struct FailureDiagnostic
{
    std::string message;
    std::source_location origin = std::source_location::current();
    std::vector<FailureDiagnosticEntry> entries;
};

[[nodiscard]] inline FailureDiagnosticEntry make_failure_diagnostic_entry(
    std::string subject_type,
    std::string subject_key = {},
    std::filesystem::path expected_path = {},
    std::filesystem::path declaration_path = {},
    std::string declaration_pointer = {},
    std::string reason = {},
    std::source_location origin = std::source_location::current())
{
    return FailureDiagnosticEntry{
        .subject_type = std::move(subject_type),
        .subject_key = std::move(subject_key),
        .expected_path = std::move(expected_path),
        .declaration_path = std::move(declaration_path),
        .declaration_pointer = std::move(declaration_pointer),
        .reason = std::move(reason),
        .origin = origin
    };
}

[[nodiscard]] inline FailureDiagnostic make_failure_diagnostic(
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    FailureDiagnostic diagnostic{
        .message = std::move(message),
        .origin = origin
    };
    if (!resource_key.empty() || !resource_path.empty())
    {
        diagnostic.entries.push_back(make_failure_diagnostic_entry(
            {},std::move(resource_key),std::move(resource_path),{},{},
            diagnostic.message,origin));
    }
    return diagnostic;
}

[[nodiscard]] inline FailureDiagnostic make_failure_diagnostic(
    std::string message,
    std::vector<FailureDiagnosticEntry> entries,
    std::source_location origin = std::source_location::current())
{
    return FailureDiagnostic{
        .message = std::move(message),
        .origin = origin,
        .entries = std::move(entries)
    };
}

[[nodiscard]] std::string format_failure_diagnostic(
    const FailureDiagnostic& diagnostic,
    std::string_view error_code,
    std::string_view category,
    const std::filesystem::path& project_root = {});

[[nodiscard]] std::string_view source_file_basename(
    const std::source_location& origin) noexcept;

[[nodiscard]] std::filesystem::path normalize_diagnostic_path(
    const std::filesystem::path& path,
    const std::filesystem::path& project_root);

void normalize_failure_diagnostic_paths(
    FailureDiagnostic& diagnostic,
    const std::filesystem::path& project_root);
}
