#include "failure_diagnostic.h"

#include <sstream>

namespace elysia::core
{
namespace
{
std::string display_path(
    const std::filesystem::path& path,
    const std::filesystem::path& project_root)
{
    if (path.empty())
        return {};
    if (!project_root.empty())
        return normalize_diagnostic_path(path,project_root).generic_string();
    return path.generic_string();
}

}

std::string_view source_file_basename(const std::source_location& origin) noexcept
{
    std::string_view file = origin.file_name() ? origin.file_name() : "";
    const auto slash = file.find_last_of("/\\");
    return slash == std::string_view::npos ? file : file.substr(slash + 1);
}

std::filesystem::path normalize_diagnostic_path(
    const std::filesystem::path& path,
    const std::filesystem::path& project_root)
{
    if (path.empty() || !path.is_absolute()) return path.lexically_normal();
    if (!project_root.empty())
    {
        const auto lexical_relative = path.lexically_normal().lexically_relative(
            project_root.lexically_normal());
        if (!lexical_relative.empty())
        {
            const auto text = lexical_relative.generic_string();
            if (text != ".." && !text.starts_with("../"))
                return lexical_relative;
        }

        std::error_code error;
        const auto relative = std::filesystem::relative(path,project_root,error);
        if (!error && !relative.empty())
        {
            const auto text = relative.generic_string();
            if (text != ".." && !text.starts_with("../"))
                return relative.lexically_normal();
        }
    }
    return path.filename();
}

void normalize_failure_diagnostic_paths(
    FailureDiagnostic& diagnostic,
    const std::filesystem::path& project_root)
{
    for (auto& entry : diagnostic.entries)
    {
        entry.expected_path = normalize_diagnostic_path(
            entry.expected_path,project_root);
        entry.declaration_path = normalize_diagnostic_path(
            entry.declaration_path,project_root);
    }
}

std::string format_failure_diagnostic(
    const FailureDiagnostic& diagnostic,
    std::string_view error_code,
    std::string_view category,
    const std::filesystem::path& project_root)
{
    std::ostringstream output;
    output << error_code;
    if (!category.empty())
        output << " [" << category << ']';
    if (!diagnostic.message.empty())
        output << '\n' << diagnostic.message;

    for (std::size_t index = 0; index < diagnostic.entries.size(); ++index)
    {
        const auto& entry = diagnostic.entries[index];
        output << "\n[" << (index + 1) << ']';
        if (!entry.subject_type.empty()) output << " type=" << entry.subject_type;
        if (!entry.subject_key.empty()) output << " key=" << entry.subject_key;
        const auto expected = display_path(entry.expected_path,project_root);
        if (!expected.empty()) output << " expected=" << expected;
        const auto declaration = display_path(entry.declaration_path,project_root);
        if (!declaration.empty()) output << " declared_at=" << declaration;
        if (!entry.declaration_pointer.empty()) output << '#' << entry.declaration_pointer;
        if (!entry.reason.empty()) output << " reason=" << entry.reason;
        output << " source=" << source_file_basename(entry.origin) << ':' << entry.origin.line();
    }
    return output.str();
}
}
