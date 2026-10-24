#pragma once

#include "../resources/texture/texture_loader.h"

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace elysia::bootstrap
{
class BootstrapTextureCache
{
public:
    BootstrapTextureCache() = default;
    ~BootstrapTextureCache() = default;

    BootstrapTextureCache(const BootstrapTextureCache&) = delete;
    BootstrapTextureCache& operator=(const BootstrapTextureCache&) = delete;
    BootstrapTextureCache(BootstrapTextureCache&&) noexcept = default;
    BootstrapTextureCache& operator=(BootstrapTextureCache&&) noexcept = default;

    [[nodiscard]] bool store(std::string key, elysia::resources::TexturePtr texture);
    [[nodiscard]] SDL_Texture* find(std::string_view key) const noexcept;
    void clear() noexcept;

private:
    std::unordered_map<std::string,elysia::resources::TexturePtr> _textures;
};
}
