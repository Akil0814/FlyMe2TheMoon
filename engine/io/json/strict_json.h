#pragma once

#include "../../../thirdparty/nlohmann/json.hpp"

#include <expected>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>

namespace elysia::io
{
using json = nlohmann::json;
enum class JsonFileError
{
    EmptyPath,
    FileMissing,
    FilesystemAccess,
    OpenFailed,
    ReadFailed,
    ParseFailed,
    DuplicateProperty
};

struct JsonFileFailure
{
    JsonFileError code = JsonFileError::ParseFailed;
    std::string message;
    std::filesystem::path file_path;
    std::string duplicate_property;
    std::string json_pointer;
    std::source_location origin = std::source_location::current();
};

[[nodiscard]] std::string json_pointer_escape(std::string_view value);

[[nodiscard]] std::expected<json,JsonFileFailure> load_strict_json(
    const std::filesystem::path& path,
    std::source_location origin = std::source_location::current());
}
