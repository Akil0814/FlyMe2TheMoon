#include "bootstrapper.h"

#include "../config/user_config_service.h"
#include "../io/loaders/content_registry_loader.h"
#include "../io/path/path_manager.h"

#include <utility>

namespace elysia::bootstrap
{
namespace
{
constexpr const char* USER_CONFIG_FILE_NAME = "user_config.json";
}

std::expected<BootstrapOutput,BootstrapFailure>
Bootstrapper::parse_runtime_settings(const std::filesystem::path& executable_path)
{
    // Reset state left over from a previous initialization attempt.
    _startup_preload_loader.reset();
    elysia::config::UserConfigService::instance()->shutdown();

    // Later stages depend on these paths to locate assets and configuration
    elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();

    if (auto path_result = path_manager->initialize(executable_path); !path_result)
    {
        // Translate a low-level filesystem error into a bootstrap-level error
        BootstrapFailure::Code code = BootstrapFailure::Code::PathAccess;
        switch (path_result.error().code)
        {
        case elysia::io::PathError::ProjectRoot:
            code = BootstrapFailure::Code::ProjectRoot;
            break;
        case elysia::io::PathError::RequiredAssetDirectory:
            code = BootstrapFailure::Code::AssetDirectory;
            break;
        case elysia::io::PathError::RuntimeDirectory:
            code = BootstrapFailure::Code::RuntimeDirectory;
            break;
        case elysia::io::PathError::FilesystemAccess:
            code = BootstrapFailure::Code::PathAccess;
            break;
        }
        // Initialization cannot continue without valid filesystem paths
        return std::unexpected(BootstrapFailure{code,std::move(path_result.error().diagnostic)});
    }

    // Ensure that required runtime directories exist.
    // These directories are needed for runtime data and user configuration.
    if (auto directory_result = path_manager->ensure_runtime_dirs(); !directory_result)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::RuntimeDirectory,
            std::move(directory_result.error().diagnostic)});

    elysia::io::ContentRegistryLoader content_registry_loader;
    auto registry_result = content_registry_loader.load(path_manager->content_registry());
    if (!registry_result)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::ContentRegistry,
            std::move(registry_result.error().diagnostic)});
    elysia::io::ContentRegistry content_registry = std::move(*registry_result);

    const auto app_config_result =_app_config_loader.load(content_registry.bootstrap.app_config);
    if (!app_config_result)
        return std::unexpected(app_config_result.error());
    const std::filesystem::path user_config_path =
        path_manager->player_data() / USER_CONFIG_FILE_NAME;

    const auto config_result =
        elysia::config::UserConfigService::instance()->initialize(
            app_config_result->user_defaults,user_config_path);

    if (!config_result)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::UserConfig,
            std::move(config_result.error().diagnostic)});

    BootstrapOutput output;

    output.runtime_settings.window_title = app_config_result->window_title;
    output.runtime_settings.user = config_result->settings;
    output.i18n_manifest_path = content_registry.required.i18n;
    output.warning = config_result->warning;
    _startup_preload_loader.set_manifest_path(content_registry.bootstrap.preload_manifest);
    output.content_registry = std::move(content_registry);

    return output;
}

std::expected<void,BootstrapFailure>
Bootstrapper::preload_startup_resources(SDL_Renderer* renderer)
{
    return _startup_preload_loader.load(renderer);
}

void Bootstrapper::release_preload_textures() noexcept
{
    _startup_preload_loader.release_textures();
}

SDL_Texture* Bootstrapper::find_preload_texture(
    std::string_view key) const noexcept
{
    return _startup_preload_loader.find_texture(key);
}
}
