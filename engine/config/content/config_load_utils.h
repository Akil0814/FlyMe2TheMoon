#pragma once

#include "../config_types.h"
#include "../../io/json/strict_json.h"
#include "../../io/path/path_manager.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace elysia::config
{
inline std::string config_project_relative(const std::filesystem::path& path)
{
    auto* paths = elysia::io::PathManager::instance();
    if (paths && paths->is_initialized())
    {
        std::error_code error;
        const auto relative = std::filesystem::relative(path,paths->root(),error);
        if (!error && !relative.empty() && *relative.begin() != "..")
            return relative.generic_string();
    }
    return path.is_absolute() ? path.filename().generic_string() : path.generic_string();
}

inline std::string config_pointer_component(std::string_view value)
{
    std::string result;
    for (char character : value)
    {
        if (character == '~') result += "~0";
        else if (character == '/') result += "~1";
        else result += character;
    }
    return result;
}

inline ConfigLoadFailure make_config_load_failure(ConfigLoadError error,std::string message,
    ConfigOrigin first = {},ConfigOrigin second = {},
    std::source_location origin = std::source_location::current())
{
    return {error,std::move(message),std::move(first),std::move(second),origin};
}
inline ConfigLoadError config_error_from_json(io::JsonFileError error) noexcept
{
    switch (error)
    {
    case io::JsonFileError::FileMissing: return ConfigLoadError::FileMissing;
    case io::JsonFileError::FilesystemAccess:
    case io::JsonFileError::ReadFailed: return ConfigLoadError::FilesystemAccess;
    case io::JsonFileError::DuplicateProperty: return ConfigLoadError::DuplicateKey;
    case io::JsonFileError::EmptyPath:
    case io::JsonFileError::OpenFailed:
    case io::JsonFileError::ParseFailed: return ConfigLoadError::OpenFailed;
    }
    return ConfigLoadError::OpenFailed;
}

inline ConfigLoadFailure config_failure_from_json(const io::JsonFileFailure& failure,ConfigOrigin origin)
{
    origin.json_pointer = failure.json_pointer;
    return make_config_load_failure(config_error_from_json(failure.code),failure.message,
        std::move(origin),{},failure.origin);
}

}
