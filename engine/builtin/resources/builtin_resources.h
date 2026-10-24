#pragma once

#include "builtin_asset_cache.h"
#include "../../tools/singleton.h"

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

struct SDL_Renderer;

namespace elysia::builtin
{
// Application explicitly initializes and shuts down SDL resources. Accessing
// instance() only constructs the empty facade; it does not load any assets.
class BuiltinResources final : public elysia::tools::Singleton<BuiltinResources>
{
    friend elysia::tools::Singleton<BuiltinResources>;
public:
    ~BuiltinResources();

    BuiltinResources(const BuiltinResources&) = delete;
    BuiltinResources& operator=(const BuiltinResources&) = delete;
    BuiltinResources(BuiltinResources&&) = delete;
    BuiltinResources& operator=(BuiltinResources&&) = delete;

    [[nodiscard]] std::expected<void, std::string> initialize(
        SDL_Renderer* renderer,
        const BuiltinAssetCatalog& catalog,
        std::span<const int> point_sizes);
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept;

    [[nodiscard]] SDL_Texture* find_texture(BuiltinTextureId id) const noexcept;
    [[nodiscard]] TTF_Font* find_font(BuiltinFontId id,int point_size) const noexcept;
    [[nodiscard]] const std::string* find_translation(
        BuiltinLocaleId locale,std::string_view key) const noexcept;
    [[nodiscard]] const BuiltinAnimationDefinition* find_animation(
        BuiltinAnimationId id) const noexcept;
    [[nodiscard]] MIX_Audio* find_sound(BuiltinSoundId id) const noexcept;
    [[nodiscard]] MIX_Audio* find_music(BuiltinMusicId id) const noexcept;
    [[nodiscard]] std::unique_ptr<elysia::animation::Animation> create_animation(
        BuiltinAnimationId id) const;

    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t font_count() const noexcept;
    [[nodiscard]] std::size_t locale_count() const noexcept;
    [[nodiscard]] std::size_t animation_count() const noexcept;
    [[nodiscard]] std::size_t sound_count() const noexcept;
    [[nodiscard]] std::size_t music_count() const noexcept;

private:
    BuiltinResources() = default;

    BuiltinAssetCache _assets;
};
}
