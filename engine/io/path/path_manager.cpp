#include "path_manager.h"

#include <optional>
#include <string>
#include <vector>

namespace elysia::io
{
namespace
{
constexpr int kMaxSearchDepth = 8;

PathFailure make_path_failure(
    PathError code,
    std::string message,
    std::vector<elysia::core::FailureDiagnosticEntry> entries = {},
    std::source_location origin = std::source_location::current())
{
    return PathFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(entries),origin)
    };
}
}

std::expected<void,PathFailure> PathManager::initialize()
{
    return initialize({});
}

std::expected<void,PathFailure> PathManager::initialize(
    const std::filesystem::path& start_path)
{
    std::filesystem::path resolved_start = start_path;
    if (resolved_start.empty())
    {
        std::error_code error;
        resolved_start = std::filesystem::current_path(error);
        if (error)
        {
            return std::unexpected(make_path_failure(
                PathError::FilesystemAccess,
                "Path manager initialization failed while reading the current working directory.",
                {elysia::core::make_failure_diagnostic_entry(
                    "working-directory",{},resolved_start,{},{},error.message())}));
        }
    }

    auto root_result = find_project_root(resolved_start);
    if (!root_result)
        return std::unexpected(std::move(root_result.error()));

    auto directories_result = validate_core_asset_dirs(*root_result);
    if (!directories_result)
        return directories_result;

    _root = std::move(*root_result);
    _initialized = true;
    return {};
}

std::expected<void,PathFailure> PathManager::ensure_runtime_dirs() const
{
    if (!_initialized)
        return std::unexpected(make_path_failure(
            PathError::RuntimeDirectory,
            "Path manager runtime directory setup failed: path manager is not initialized."));

    for (const auto& directory : {player_data(),saves(),logs()})
    {
        std::error_code error;
        std::filesystem::create_directories(directory,error);
        std::error_code status_error;
        const bool directory_exists = std::filesystem::is_directory(directory,status_error);
        if (error || status_error || !directory_exists)
        {
            return std::unexpected(make_path_failure(
                PathError::RuntimeDirectory,
                "Path manager runtime directory setup failed.",
                {elysia::core::make_failure_diagnostic_entry(
                    "runtime-directory",{},directory,{},{},
                    error ? error.message()
                        : status_error ? status_error.message() : "path is not a directory")}));
        }
    }
    return {};
}

std::expected<void,PathFailure> PathManager::validate_core_asset_dirs(
    const std::filesystem::path& root) const
{
    const std::filesystem::path assets_root = root / "assets";
    const std::pair<const char*,std::filesystem::path> directories[]{
        {"audio",assets_root / "audio"},
        {"textures",assets_root / "textures"},
        {"fonts",assets_root / "fonts"},
        {"configs",assets_root / "configs"}
    };
    for (const auto& [name,path] : directories)
    {
        std::error_code error;
        const bool is_directory = std::filesystem::is_directory(path,error);
        const bool missing = error == std::errc::no_such_file_or_directory;
        if (missing) error.clear();
        if (error || !is_directory)
        {
            return std::unexpected(make_path_failure(
                error ? PathError::FilesystemAccess : PathError::RequiredAssetDirectory,
                "Path manager initialization failed: a required asset directory is unavailable.",
                {elysia::core::make_failure_diagnostic_entry(
                    "asset-directory",name,path,{},{},
                    error ? error.message() : "directory does not exist")}));
        }
    }
    return {};
}

const std::filesystem::path& PathManager::root() const { return _root; }
std::filesystem::path PathManager::assets() const { return _root / "assets"; }
std::filesystem::path PathManager::configs() const { return assets() / "configs"; }
std::filesystem::path PathManager::fonts() const { return assets() / "fonts"; }
std::filesystem::path PathManager::preload() const { return assets() / "preload"; }
std::filesystem::path PathManager::audio() const { return assets() / "audio"; }
std::filesystem::path PathManager::textures() const { return assets() / "textures"; }
std::filesystem::path PathManager::player_data() const { return _root / "player_data"; }
std::filesystem::path PathManager::saves() const { return player_data() / "saves"; }
std::filesystem::path PathManager::logs() const { return _root / "logs"; }
std::filesystem::path PathManager::content_registry() const { return assets() / "content_registry.json"; }

std::filesystem::path PathManager::to_project_path(const std::filesystem::path& path) const
{
    if (path.is_absolute()) return path.lexically_normal();
    return (_root / path).lexically_normal();
}

std::filesystem::path PathManager::to_asset_path(const std::filesystem::path& path) const
{
    if (path.is_absolute()) return path.lexically_normal();
    if (path_starts_with(path,"assets")) return to_project_path(path);
    return (assets() / path).lexically_normal();
}

std::filesystem::path PathManager::to_config_path(const std::filesystem::path& path) const
{
    if (path.is_absolute()) return path.lexically_normal();
    if (path_starts_with(path,"assets")) return to_project_path(path);
    if (path_starts_with(path,"configs")) return to_asset_path(path);
    return (configs() / path).lexically_normal();
}

bool PathManager::path_starts_with(
    const std::filesystem::path& path,const std::string& first_part) const
{
    const auto iterator = path.begin();
    return iterator != path.end() && iterator->string() == first_part;
}

std::expected<std::filesystem::path,PathFailure>
PathManager::find_project_root(const std::filesystem::path& start_path) const
{
    std::error_code error;
    std::filesystem::path current = std::filesystem::absolute(start_path,error);
    if (error)
    {
        return std::unexpected(make_path_failure(
            PathError::FilesystemAccess,
            "Project root search failed while resolving the start path.",
            {elysia::core::make_failure_diagnostic_entry(
                "start-path",{},start_path,{},{},error.message())}));
    }
    if (!std::filesystem::is_directory(current,error))
        current = current.parent_path();
    if (error)
    {
        return std::unexpected(make_path_failure(
            PathError::FilesystemAccess,
            "Project root search failed while inspecting the start path.",
            {elysia::core::make_failure_diagnostic_entry(
                "start-path",{},current,{},{},error.message())}));
    }

    std::optional<std::filesystem::path> marker_root;
    std::vector<elysia::core::FailureDiagnosticEntry> searched;
    for (int depth = 0; depth < kMaxSearchDepth; ++depth)
    {
        const auto marker = current / "assets" / ".elysia_root";
        const auto registry = current / "assets" / "content_registry.json";
        searched.push_back(elysia::core::make_failure_diagnostic_entry(
            "project-marker",{},marker,{},{},"marker is not a regular file"));

        error.clear();
        const auto marker_status = std::filesystem::status(marker,error);
        const bool marker_missing = error == std::errc::no_such_file_or_directory;
        if (marker_missing) error.clear();
        const bool has_marker = !error && std::filesystem::is_regular_file(marker_status);
        if (error)
        {
            searched.back().reason = error.message();
            return std::unexpected(make_path_failure(
                PathError::FilesystemAccess,
                "Project root search failed while checking the project marker.",
                std::move(searched)));
        }
        if (has_marker)
        {
            error.clear();
            const auto registry_status = std::filesystem::status(registry,error);
            const bool registry_missing = error == std::errc::no_such_file_or_directory;
            if (registry_missing) error.clear();
            const bool has_registry = !error && std::filesystem::is_regular_file(registry_status);
            if (error)
            {
                searched.back().reason = error.message();
                return std::unexpected(make_path_failure(
                    PathError::FilesystemAccess,
                    "Project root search failed while checking the content registry.",
                    std::move(searched)));
            }
            if (has_registry)
                return current;
            if (!marker_root)
                marker_root = current;
        }

        if (current == current.root_path())
            break;
        current = current.parent_path();
    }
    if (marker_root)
        return *marker_root;

    return std::unexpected(make_path_failure(
        PathError::ProjectRoot,
        "Required Elysia project marker was not found.",
        std::move(searched)));
}
}
