#pragma once

#include "builtin_asset_catalog.h"
#include "../../animation/animation.h"
#include "../../resources/atlas/atlas.h"
#include "../../resources/texture/texture_loader.h"

#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <expected>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

struct SDL_Renderer;

namespace elysia::builtin
{
struct BuiltinFontDeleter
{
    void operator()(TTF_Font* font) const noexcept;
};

struct BuiltinSoundDeleter
{
    void operator()(MIX_Audio* sound) const noexcept;
};

struct BuiltinMusicDeleter
{
    void operator()(MIX_Audio* music) const noexcept;
};

using BuiltinFontPtr = std::unique_ptr<TTF_Font, BuiltinFontDeleter>;
using BuiltinSoundPtr = std::unique_ptr<MIX_Audio, BuiltinSoundDeleter>;
using BuiltinMusicPtr = std::unique_ptr<MIX_Audio, BuiltinMusicDeleter>;
using BuiltinTranslationTable = std::unordered_map<std::string, std::string>;

struct BuiltinAnimationDefinition
{
    BuiltinAnimationId id = BuiltinAnimationId::Count;
    const elysia::resources::Atlas* atlas = nullptr;
    double fps = 0.0;
    bool loop = false;
};

class BuiltinAssetCache
{
public:
    BuiltinAssetCache() = default;
    ~BuiltinAssetCache();

    BuiltinAssetCache(const BuiltinAssetCache&) = delete;
    BuiltinAssetCache& operator=(const BuiltinAssetCache&) = delete;
    BuiltinAssetCache(BuiltinAssetCache&&) = delete;
    BuiltinAssetCache& operator=(BuiltinAssetCache&&) = delete;

    [[nodiscard]] std::expected<void, std::string> initialize(SDL_Renderer* renderer,
        const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes);
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
    using TextureStorage = std::array<elysia::resources::TextureResource,
        builtin_resource_index(BuiltinTextureId::Count)>;
    using FontSizeMap = std::unordered_map<int, BuiltinFontPtr>;
    using FontStorage = std::array<FontSizeMap,
        builtin_resource_index(BuiltinFontId::Count)>;
    using TranslationTables = std::array<BuiltinTranslationTable,
        builtin_resource_index(BuiltinLocaleId::Count)>;
    using AtlasStorage = std::array<std::unique_ptr<elysia::resources::Atlas>,
        builtin_resource_index(BuiltinAnimationId::Count)>;
    using AnimationDefinitions = std::array<std::optional<BuiltinAnimationDefinition>,
        builtin_resource_index(BuiltinAnimationId::Count)>;
    using SoundStorage = std::array<BuiltinSoundPtr,
        builtin_resource_index(BuiltinSoundId::Count)>;
    using MusicStorage = std::array<BuiltinMusicPtr,
        builtin_resource_index(BuiltinMusicId::Count)>;

    struct PreparedState
    {
        TextureStorage textures;
        FontStorage fonts;
        TranslationTables translations;
        AtlasStorage atlases;
        AnimationDefinitions animations;
        SoundStorage sounds;
        MusicStorage music;
    };

    [[nodiscard]] std::expected<PreparedState, std::string> prepare(SDL_Renderer* renderer,
        const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes) const;

private:
    TextureStorage _textures;
    FontStorage _fonts;
    TranslationTables _translations;
    AtlasStorage _atlases;
    AnimationDefinitions _animations;
    SoundStorage _sounds;
    MusicStorage _music;
    SDL_Renderer* _renderer = nullptr;
};
}
