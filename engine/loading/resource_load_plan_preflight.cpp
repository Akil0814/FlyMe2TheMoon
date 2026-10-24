#include "resource_load_plan_preflight.h"

#include "resource_load_plan.h"
#include "../io/path/path_manager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace elysia::loading
{
namespace
{
std::filesystem::path relative_to_project(const std::filesystem::path& path)
{
    auto* paths = elysia::io::PathManager::instance();
    return elysia::core::normalize_diagnostic_path(
        path,paths && paths->is_initialized() ? paths->root() : std::filesystem::path{});
}

void add_missing(
    std::vector<elysia::core::FailureDiagnosticEntry>& missing,
    std::string type,std::string key,const std::filesystem::path& path,
    const elysia::resources::ResourceOrigin& resource_origin,
    std::string reason,
    std::source_location origin = std::source_location::current())
{
    missing.push_back(elysia::core::make_failure_diagnostic_entry(
        std::move(type),std::move(key),relative_to_project(path),
        relative_to_project(resource_origin.config_path),resource_origin.json_pointer,
        std::move(reason),origin));
}

std::expected<bool,ContentLoadFailure> probe_path(
    const std::filesystem::path& path,bool expect_directory,
    std::string type,std::string key,
    const elysia::resources::ResourceOrigin& resource_origin,
    std::source_location origin = std::source_location::current())
{
    std::error_code error;
    const bool matches = expect_directory
        ? std::filesystem::is_directory(path,error)
        : std::filesystem::is_regular_file(path,error);
    if (matches) return true;
    if (!error || error == std::errc::no_such_file_or_directory) return false;

    auto diagnostic = elysia::core::make_failure_diagnostic(
        "Content resource status could not be read.",
        {elysia::core::make_failure_diagnostic_entry(
            std::move(type),std::move(key),relative_to_project(path),
            relative_to_project(resource_origin.config_path),
            resource_origin.json_pointer,error.message(),origin)},origin);
    return std::unexpected(make_content_load_failure(
        ContentLoadError::Plan,std::move(diagnostic)));
}

std::filesystem::path frame_path(
    const elysia::resources::AtlasBuildRequest& request,std::size_t index)
{
    std::ostringstream name;
    name << request.frame_filename_prefix << '_' << std::setw(3)
        << std::setfill('0') << index << ".png";
    return request.source_path / name.str();
}
}

std::expected<void,ContentLoadFailure> ResourceLoadPlanPreflight::validate(
    const ResourceLoadPlan& plan,
    std::vector<elysia::core::FailureDiagnosticEntry> missing) const
{
    for (auto& entry : missing)
    {
        entry.expected_path = relative_to_project(entry.expected_path);
        entry.declaration_path = relative_to_project(entry.declaration_path);
    }
    const auto check = [&](std::string type,std::string key,
        const std::filesystem::path& path,
        const elysia::resources::ResourceOrigin& resource_origin,
        bool expect_directory = false) -> std::expected<void,ContentLoadFailure>
    {
        auto probe = probe_path(path,expect_directory,type,key,resource_origin);
        if (!probe) return std::unexpected(std::move(probe.error()));
        if (!*probe)
            add_missing(missing,std::move(type),std::move(key),path,resource_origin,
                expect_directory ? "directory does not exist or is not a directory"
                                 : "file does not exist or is not a regular file");
        return {};
    };
    for (const auto& request : plan.texture_requests())
        if (auto result = check("texture",request.key,request.file_path,request.origin); !result)
            return result;
    for (const auto& request : plan.font_requests())
        if (auto result = check("font",request.key,request.file_path,request.origin); !result)
            return result;
    for (const auto& request : plan.sound_requests())
        if (auto result = check("sound",request.key,request.file_path,request.origin); !result)
            return result;
    for (const auto& request : plan.music_requests())
        if (auto result = check("music",request.key,request.file_path,request.origin); !result)
            return result;
    for (const auto& request : plan.atlas_build_requests())
    {
        if (request.source_type == elysia::resources::AtlasSourceType::HorizontalStrip)
        {
            if (auto result = check("atlas-strip",request.atlas_key,
                request.source_path,request.origin); !result) return result;
            continue;
        }
        if (auto result = check("atlas-directory",request.atlas_key,
            request.source_path,request.origin,true); !result) return result;
        for (std::size_t index = 0; index < request.frame_count; ++index)
        {
            const auto expected = frame_path(request,index);
            if (auto result = check("atlas-frame",request.atlas_key,
                expected,request.origin); !result) return result;
        }
    }
    if (missing.empty()) return {};

    std::sort(missing.begin(),missing.end(),[](const auto& left,const auto& right)
    {
        return std::tie(left.subject_type,left.subject_key,left.expected_path)
            < std::tie(right.subject_type,right.subject_key,right.expected_path);
    });
    return std::unexpected(make_content_load_failure(
        ContentLoadError::MissingResource,
        elysia::core::make_failure_diagnostic(
            "One or more required content resources are missing.",std::move(missing))));
}
}
