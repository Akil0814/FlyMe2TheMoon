#include "content_registry_loader.h"
#include "detail/content_registry_json_failure.h"

#include "../json/json_loader.h"
#include "../path/path_manager.h"

#include <array>
#include <string>
#include <string_view>

namespace elysia::io
{
namespace
{
constexpr std::array<std::string_view,2> bootstrap_keys{"app_config","preload_manifest"};
constexpr std::array<std::string_view,7> required_keys{
    "configs","fonts","audio","i18n","textures","animations","effects"};

ContentRegistryFailure failure(
    ContentRegistryError code,std::string message,
    const std::filesystem::path& registry,std::string pointer = {},
    std::filesystem::path expected = {},
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    return ContentRegistryFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),
            {elysia::core::make_failure_diagnostic_entry(
                "content-registry",{},std::move(expected),registry,
                std::move(pointer),reason,origin)},origin)
    };
}

template<std::size_t Size>
bool contains(const std::array<std::string_view,Size>& keys,std::string_view key)
{
    for (const auto known : keys) if (known == key) return true;
    return false;
}

std::expected<std::filesystem::path,ContentRegistryFailure> read_required_path(
    const json& object,std::string section,std::string key,
    PathManager& paths,const std::filesystem::path& registry)
{
    const std::string pointer = "/" + section + "/" + key;
    if (!object.contains(key) || !object.at(key).is_string())
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry path is missing or invalid: " + key,
            registry,pointer));
    const auto path = paths.to_asset_path(object.at(key).get<std::string>());
    std::error_code error;
    if (!std::filesystem::is_regular_file(path,error))
    {
        if (error && error != std::errc::no_such_file_or_directory)
            return std::unexpected(failure(ContentRegistryError::FilesystemAccess,
                "Content registry referenced file access failed: " + error.message(),
                registry,pointer,path));
        return std::unexpected(failure(ContentRegistryError::MissingReferencedFile,
            "Content registry references a required file that does not exist.",
            registry,pointer,path));
    }
    return path;
}
}

std::expected<ContentRegistry,ContentRegistryFailure> ContentRegistryLoader::load(
    const std::filesystem::path& registry_path) const
{
    auto* paths = PathManager::instance();
    if (!paths || !paths->is_initialized())
        return std::unexpected(failure(ContentRegistryError::UnavailableDependency,
            "Content registry loading requires an initialized path manager.",
            registry_path));
    std::error_code error;
    if (!std::filesystem::is_regular_file(registry_path,error))
    {
        if (error && error != std::errc::no_such_file_or_directory)
            return std::unexpected(failure(ContentRegistryError::FilesystemAccess,
                "Content registry file access failed: " + error.message(),
                registry_path,{},registry_path));
        return std::unexpected(failure(ContentRegistryError::FileMissing,
            "Content registry file does not exist or is not a regular file.",
            registry_path,{},registry_path));
    }
    JsonLoader loader;
    const auto read = loader.open_file(registry_path);
    if (!read)
    {
        return std::unexpected(detail::registry_failure_from_json(read.error()));
    }
    if (!loader.root().is_object())
        return std::unexpected(failure(ContentRegistryError::InvalidDocument,
            "Content registry root is not an object.",registry_path));
    const json& root = loader.root();
    if (root.size() != 2 || !root.contains("bootstrap") || !root.contains("manifests"))
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry root must contain bootstrap and manifests.",registry_path,"/"));
    for (auto item = root.begin(); item != root.end(); ++item)
        if (item.key() != "bootstrap" && item.key() != "manifests")
            return std::unexpected(failure(ContentRegistryError::InvalidSchema,
                "Content registry contains an unknown root key: " + item.key(),
                registry_path,"/" + item.key()));

    const json& bootstrap = root.at("bootstrap");
    if (!bootstrap.is_object())
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry bootstrap is not an object.",registry_path,"/bootstrap"));
    for (auto item = bootstrap.begin(); item != bootstrap.end(); ++item)
        if (!contains(bootstrap_keys,item.key()))
            return std::unexpected(failure(ContentRegistryError::InvalidSchema,
                "Content registry contains an unknown bootstrap key: " + item.key(),
                registry_path,"/bootstrap/" + item.key()));

    const json& manifests = root.at("manifests");
    if (!manifests.is_object() || !manifests.contains("required"))
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry manifests.required is missing.",registry_path,"/manifests/required"));
    for (auto item = manifests.begin(); item != manifests.end(); ++item)
        if (item.key() != "required" && item.key() != "additional")
            return std::unexpected(failure(ContentRegistryError::InvalidSchema,
                "Content registry contains an unknown manifests key: " + item.key(),
                registry_path,"/manifests/" + item.key()));
    const json& required = manifests.at("required");
    if (!required.is_object())
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry manifests.required is not an object.",registry_path,"/manifests/required"));
    for (auto item = required.begin(); item != required.end(); ++item)
        if (!contains(required_keys,item.key()))
            return std::unexpected(failure(ContentRegistryError::InvalidSchema,
                "Content registry contains an unknown required key: " + item.key(),
                registry_path,"/manifests/required/" + item.key()));

    ContentRegistry output;
    auto app = read_required_path(bootstrap,"bootstrap","app_config",*paths,registry_path);
    if (!app) return std::unexpected(std::move(app.error()));
    output.bootstrap.app_config = std::move(*app);
    auto preload = read_required_path(bootstrap,"bootstrap","preload_manifest",*paths,registry_path);
    if (!preload) return std::unexpected(std::move(preload.error()));
    output.bootstrap.preload_manifest = std::move(*preload);

    struct Target { const char* key; std::filesystem::path CoreManifestPaths::* member; };
    constexpr Target targets[]{
        {"configs",&CoreManifestPaths::configs},{"fonts",&CoreManifestPaths::fonts},
        {"audio",&CoreManifestPaths::audio},{"i18n",&CoreManifestPaths::i18n},
        {"textures",&CoreManifestPaths::textures},{"animations",&CoreManifestPaths::animations},
        {"effects",&CoreManifestPaths::effects}
    };
    for (const auto& target : targets)
    {
        auto path = read_required_path(required,"manifests/required",target.key,*paths,registry_path);
        if (!path) return std::unexpected(std::move(path.error()));
        output.required.*(target.member) = std::move(*path);
    }

    if (!manifests.contains("additional")) return output;
    const json& additional = manifests.at("additional");
    if (!additional.is_object())
        return std::unexpected(failure(ContentRegistryError::InvalidSchema,
            "Content registry manifests.additional is not an object.",registry_path,"/manifests/additional"));
    for (auto item = additional.begin(); item != additional.end(); ++item)
    {
        const std::string pointer = "/manifests/additional/" + item.key();
        if (!item.value().is_string())
            return std::unexpected(failure(ContentRegistryError::InvalidSchema,
                "Additional module manifest is not a path string: " + item.key(),registry_path,pointer));
        const auto path = paths->to_asset_path(item.value().get<std::string>());
        error.clear();
        if (!std::filesystem::is_regular_file(path,error))
        {
            if (error && error != std::errc::no_such_file_or_directory)
                return std::unexpected(failure(ContentRegistryError::FilesystemAccess,
                    "Additional module manifest access failed: " + error.message(),
                    registry_path,pointer,path));
            return std::unexpected(failure(ContentRegistryError::MissingReferencedFile,
                "Additional module manifest does not exist: " + item.key(),registry_path,pointer,path));
        }
        output.additional_module_manifests.emplace(item.key(),path);
    }
    return output;
}
}
