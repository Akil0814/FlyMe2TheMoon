#pragma once

#include "app_config_loader.h"
#include "bootstrap_types.h"
#include "startup_preload_loader.h"
#include "../tools/singleton.h"

#include <SDL3/SDL.h>

#include <expected>
#include <filesystem>
#include <string_view>

namespace elysia::bootstrap
{
class Bootstrapper : public elysia::tools::Singleton<Bootstrapper>
{
    friend elysia::tools::Singleton<Bootstrapper>;

public:
    [[nodiscard]] std::expected<BootstrapOutput,BootstrapFailure>
        parse_runtime_settings(const std::filesystem::path& executable_path);

    [[nodiscard]] std::expected<void,BootstrapFailure>
        preload_startup_resources(SDL_Renderer* renderer);

    void release_preload_textures() noexcept;

    [[nodiscard]] SDL_Texture* find_preload_texture(
        std::string_view key) const noexcept;

private:
    AppConfigLoader _app_config_loader;
    StartupPreloadLoader _startup_preload_loader;
};
}
