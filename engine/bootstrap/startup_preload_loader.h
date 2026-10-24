#pragma once

#include "bootstrap_types.h"
#include "bootstrap_texture_cache.h"
#include "../io/json/json_loader.h"
#include "../resources/resource_origin.h"

#include <SDL3/SDL.h>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::bootstrap
{
class StartupPreloadLoader
{
public:
    void set_manifest_path(const std::filesystem::path& preload_manifest_path);
    void reset();
    void release_textures() noexcept;

    [[nodiscard]] std::expected<void,BootstrapFailure>load(SDL_Renderer* renderer);
    [[nodiscard]] SDL_Texture* find_texture(std::string_view key) const noexcept;

private:
    struct TextureEntry
    {
        std::string key;
        std::filesystem::path file;
        elysia::resources::ResourceOrigin origin;
    };

    [[nodiscard]] std::expected<void,BootstrapFailure> load_manifest();
    [[nodiscard]] std::expected<void,BootstrapFailure> load_textures(SDL_Renderer* renderer,BootstrapTextureCache& destination);
    [[nodiscard]] std::expected<void,BootstrapFailure> load_texture(SDL_Renderer* renderer,std::string_view key,
        const std::filesystem::path& file,const elysia::resources::ResourceOrigin& origin,
        BootstrapTextureCache& destination);

private:
    elysia::io::JsonLoader _manifest_loader;
    std::filesystem::path _manifest_path;
    std::vector<TextureEntry> _project_textures;
    BootstrapTextureCache _texture_cache;
    SDL_Renderer* _renderer = nullptr;
    bool _is_loaded = false;
};

}
