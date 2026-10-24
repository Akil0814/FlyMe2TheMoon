#include "bootstrap_texture_cache.h"

#include <utility>

namespace elysia::bootstrap
{
bool BootstrapTextureCache::store(
    std::string key,
    elysia::resources::TexturePtr texture
)
{
    if (key.empty() || !texture || _textures.contains(key))
        return false;

    _textures.emplace(std::move(key),std::move(texture));
    return true;
}

SDL_Texture* BootstrapTextureCache::find(std::string_view key) const noexcept
{
    if (key.empty())
        return nullptr;

    const auto found = _textures.find(std::string(key));
    return found == _textures.end() ? nullptr : found->second.get();
}

void BootstrapTextureCache::clear() noexcept
{
    _textures.clear();
}
}
