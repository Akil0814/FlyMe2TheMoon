#include "engine/audio/mixer_backend.h"
#include "engine/core/render/sdl_texture_size.h"
#include "builtin_asset_cache.h"

#include "../../io/json/strict_json.h"
#include "../../resources/texture/surface_loader.h"
#include "../../resources/texture/texture_loader.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace elysia::builtin
{
namespace
{
bool flatten_translation_json(const elysia::io::json& node,const std::string& prefix,
    BuiltinTranslationTable& destination)
{
    if (node.is_string())
    {
        if (prefix.empty() || node.get_ref<const std::string&>().empty())
            return false;

        return destination.emplace(prefix, node.get<std::string>()).second;
    }

    if (!node.is_object() || node.empty())
        return false;

    for (auto iterator = node.begin(); iterator != node.end(); ++iterator)
    {
        const std::string child_prefix = prefix.empty()? iterator.key(): prefix + "." + iterator.key();
        if (!flatten_translation_json(iterator.value(), child_prefix, destination))
            return false;
    }

    return true;
}

std::string make_prepare_error(std::string_view operation, const std::filesystem::path& path)
{
    return std::string(operation) + ": " + path.string();
}

template<typename Id>
bool valid_id(Id id, Id count) noexcept
{
    return builtin_resource_index(id) < builtin_resource_index(count);
}
}

void BuiltinFontDeleter::operator()(TTF_Font* font) const noexcept
{
    if (font)
        TTF_CloseFont(font);
}

void BuiltinSoundDeleter::operator()(MIX_Audio* sound) const noexcept
{
    if (sound)
        elysia::audio::detail::mixer_backend().destroy_audio(sound);
}

void BuiltinMusicDeleter::operator()(MIX_Audio* music) const noexcept
{
    if (music)
        elysia::audio::detail::mixer_backend().destroy_audio(music);
}

BuiltinAssetCache::~BuiltinAssetCache()
{
    shutdown();
}

std::expected<void, std::string> BuiltinAssetCache::initialize(
    SDL_Renderer* renderer,const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes)
{
    if (!renderer)
        return std::unexpected("Built-in asset cache initialization failed: renderer is null.");

    if (_renderer && _renderer != renderer)
        return std::unexpected(
        "Built-in asset cache initialization failed: renderer changed; shutdown is required first.");

        auto prepared = prepare(renderer, catalog, point_sizes);
        if (!prepared)
            return std::unexpected(prepared.error());

        _textures = std::move(prepared->textures);
        _fonts = std::move(prepared->fonts);
        _translations = std::move(prepared->translations);
        _atlases = std::move(prepared->atlases);
        _animations = std::move(prepared->animations);
        _sounds = std::move(prepared->sounds);
        _music = std::move(prepared->music);
        _renderer = renderer;
        return {};
}

void BuiltinAssetCache::shutdown() noexcept
{
    _music = {};
    _sounds = {};
    _animations = {};
    _atlases = {};
    _translations = {};
    _fonts = {};
    _textures = {};
    _renderer = nullptr;
}

bool BuiltinAssetCache::is_initialized() const noexcept
{
    return _renderer != nullptr;
}

SDL_Texture* BuiltinAssetCache::find_texture(BuiltinTextureId id) const noexcept
{
    if (!valid_id(id, BuiltinTextureId::Count))
        return nullptr;
    return _textures[builtin_resource_index(id)].texture.get();
}

TTF_Font* BuiltinAssetCache::find_font(BuiltinFontId id,int point_size) const noexcept
{
    if (!valid_id(id, BuiltinFontId::Count))
        return nullptr;

    const auto& sizes = _fonts[builtin_resource_index(id)];
    const auto found = sizes.find(point_size);
    return found == sizes.end() ? nullptr : found->second.get();
}

const std::string* BuiltinAssetCache::find_translation(
    BuiltinLocaleId locale,std::string_view key) const noexcept
{
    if (!valid_id(locale, BuiltinLocaleId::Count))
        return nullptr;

    const auto& table = _translations[builtin_resource_index(locale)];
    const auto translation_found = table.find(std::string(key));
    return translation_found == table.end()
        ? nullptr
        : &translation_found->second;
}

const BuiltinAnimationDefinition* BuiltinAssetCache::find_animation(
    BuiltinAnimationId id) const noexcept
{
    if (!valid_id(id, BuiltinAnimationId::Count))
        return nullptr;
    const auto& definition = _animations[builtin_resource_index(id)];
    return definition ? &*definition : nullptr;
}

MIX_Audio* BuiltinAssetCache::find_sound(BuiltinSoundId id) const noexcept
{
    if (!valid_id(id, BuiltinSoundId::Count))
        return nullptr;
    return _sounds[builtin_resource_index(id)].get();
}

MIX_Audio* BuiltinAssetCache::find_music(BuiltinMusicId id) const noexcept
{
    if (!valid_id(id, BuiltinMusicId::Count))
        return nullptr;
    return _music[builtin_resource_index(id)].get();
}

std::unique_ptr<elysia::animation::Animation> BuiltinAssetCache::create_animation(
    BuiltinAnimationId id) const
{
    const BuiltinAnimationDefinition* definition = find_animation(id);
    if (!definition || !definition->atlas || definition->fps <= 0.0)
        return nullptr;

    auto animation = std::make_unique<elysia::animation::Animation>();
    animation->set_atlas(definition->atlas);
    animation->set_loop(definition->loop);
    animation->set_interval_seconds(1.0 / definition->fps);
    return animation;
}

std::size_t BuiltinAssetCache::texture_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        _textures,
        [](const auto& texture) { return texture.texture != nullptr; }));
}

std::size_t BuiltinAssetCache::font_count() const noexcept
{
    std::size_t count = 0;
    for (const auto& sizes : _fonts)
        count += sizes.size();
    return count;
}

std::size_t BuiltinAssetCache::locale_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        _translations,
        [](const auto& table) { return !table.empty(); }));
}

std::size_t BuiltinAssetCache::animation_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        _animations,
        [](const auto& definition) { return definition.has_value(); }));
}

std::size_t BuiltinAssetCache::sound_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        _sounds,
        [](const auto& sound) { return sound != nullptr; }));
}

std::size_t BuiltinAssetCache::music_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        _music,
        [](const auto& music) { return music != nullptr; }));
}

std::expected<BuiltinAssetCache::PreparedState, std::string> BuiltinAssetCache::prepare(
    SDL_Renderer* renderer,const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes) const
{
    if (const auto validation = catalog.validate_required_files(); !validation)
    {
        return std::unexpected(
            "Built-in required resource validation failed: " + validation.error().path.string());
    }

    if (point_sizes.empty())
    {
        return std::unexpected(
            "Built-in font initialization requires at least one point size.");
    }

    std::vector<int> normalized_point_sizes(point_sizes.begin(),point_sizes.end());
    if (std::ranges::any_of(
            normalized_point_sizes,
            [](int point_size) { return point_size <= 0; }))
    {
        return std::unexpected(
            "Built-in font point sizes must be positive.");
    }

    std::ranges::sort(normalized_point_sizes);
    normalized_point_sizes.erase(
        std::unique(
            normalized_point_sizes.begin(),
            normalized_point_sizes.end()),
            normalized_point_sizes.end());

    PreparedState prepared;
    elysia::resources::SurfaceLoader surface_loader;
    elysia::resources::TextureLoader texture_loader;
    std::array<bool, builtin_resource_index(BuiltinTextureId::Count)>
        animation_textures{};

    for (const BuiltinAnimationDescriptor& descriptor : catalog.animations())
    {
        if (!valid_id(descriptor.texture_id, BuiltinTextureId::Count))
            return std::unexpected("Built-in animation texture id is invalid.");
        animation_textures[builtin_resource_index(descriptor.texture_id)] = true;
    }

    for (const BuiltinTextureDescriptor& descriptor : catalog.textures())
    {
        if (!valid_id(descriptor.id, BuiltinTextureId::Count))
            return std::unexpected("Built-in texture id is invalid.");
        const std::size_t texture_index = builtin_resource_index(descriptor.id);
        if (prepared.textures[texture_index].texture)
        {
            return std::unexpected(
                "Built-in texture id is duplicated: "
                + std::string(builtin_resource_name(descriptor.id)));
        }

        const std::filesystem::path path = catalog.resolve(descriptor.relative_path);
        auto surface = surface_loader.load_surface({
            ._asset_key = std::string(builtin_resource_name(descriptor.id)),
            ._frame_path = path,
            ._frame_index = 0
        });

        if (!surface)
            return std::unexpected(surface.error().diagnostic.message);

        auto texture = texture_loader.load_texture(renderer,*surface);
        if (!texture || !texture->_texture)
            return std::unexpected(texture
                ? make_prepare_error("Built-in texture creation failed",path)
                : texture.error().diagnostic.message);

        elysia::resources::TextureResource texture_resource{
            .texture = std::move(texture->_texture)
        };

        if (animation_textures[texture_index])
        {
            auto coverage_mask_surface =
                elysia::resources::create_coverage_mask_surface(*surface->_surface);
            if (!coverage_mask_surface)
                return std::unexpected(make_prepare_error(
                    "Built-in animation coverage mask surface creation failed",
                    path));

            texture_resource.coverage_mask = texture_loader.create_texture(
                renderer,
                **coverage_mask_surface);
            if (!texture_resource.coverage_mask
                || !SDL_SetTextureBlendMode(
                    texture_resource.coverage_mask.get(),
                    SDL_BLENDMODE_BLEND))
            {
                return std::unexpected(make_prepare_error(
                    "Built-in animation coverage mask texture creation failed",
                    path));
            }
        }
        prepared.textures[texture_index] = std::move(texture_resource);
    }

    for (const BuiltinFontDescriptor& descriptor : catalog.fonts())
    {
        if (!valid_id(descriptor.id, BuiltinFontId::Count)
            || !valid_id(descriptor.locale, BuiltinLocaleId::Count)
            || builtin_font_id(descriptor.locale) != descriptor.id)
        {
            return std::unexpected("Built-in font descriptor id is invalid.");
        }

        const std::filesystem::path path = catalog.resolve(descriptor.relative_path);
        auto& sizes = prepared.fonts[builtin_resource_index(descriptor.id)];
        for (const int point_size : normalized_point_sizes)
        {
            TTF_Font* raw_font = TTF_OpenFont(path.string().c_str(), point_size);
            if (!raw_font)
                return std::unexpected(make_prepare_error("Built-in font load failed", path));
            if (!sizes.emplace(point_size, BuiltinFontPtr(raw_font)).second)
            {
                return std::unexpected(
                    "Built-in font size is duplicated: "
                    + std::string(builtin_resource_name(descriptor.id)));
            }
        }
    }

    for (const BuiltinAnimationDescriptor& descriptor : catalog.animations())
    {
        if (!valid_id(descriptor.id, BuiltinAnimationId::Count))
            return std::unexpected("Built-in animation id is invalid.");

        const std::size_t animation_index = builtin_resource_index(descriptor.id);
        auto& texture = prepared.textures[
            builtin_resource_index(descriptor.texture_id)];
        if (!texture.texture || !texture.coverage_mask)
        {
            return std::unexpected(
                "Built-in animation texture is not registered: "
                + std::string(builtin_resource_name(descriptor.texture_id)));
        }

        int texture_width = 0;
        int texture_height = 0;
        if (!elysia::core::texture_pixel_size(texture.texture.get(),&texture_width,&texture_height)
            || !descriptor.has_expected_texture_dimensions(texture_width, texture_height))
        {
            return std::unexpected(
                "Built-in animation texture dimensions are invalid: "
                + std::string(builtin_resource_name(descriptor.id)));
        }

        auto atlas = std::make_unique<elysia::resources::Atlas>(
            std::string(builtin_resource_name(descriptor.id)));
        for (std::size_t frame_index = 0; frame_index < descriptor.frame_count; ++frame_index)
        {
            const elysia::core::Rect source_rect(
                static_cast<float>(frame_index * static_cast<std::size_t>(descriptor.frame_width)),
                0.0f,
                static_cast<float>(descriptor.frame_width),
                static_cast<float>(descriptor.frame_height));
            if (!atlas->add_frame(
                {},
                texture.texture.get(),
                texture.coverage_mask.get(),
                source_rect))
            {
                return std::unexpected(
                    "Built-in animation atlas build failed: "
                    + std::string(builtin_resource_name(descriptor.id)));
            }
        }

        if (prepared.atlases[animation_index]
            || prepared.animations[animation_index])
        {
            return std::unexpected(
                "Built-in animation id is duplicated: "
                + std::string(builtin_resource_name(descriptor.id)));
        }

        const elysia::resources::Atlas* atlas_pointer = atlas.get();
        prepared.atlases[animation_index] = std::move(atlas);
        prepared.animations[animation_index] = BuiltinAnimationDefinition{
            .id = descriptor.id,
            .atlas = atlas_pointer,
            .fps = descriptor.fps,
            .loop = descriptor.loop
        };
    }

    for (const BuiltinLocaleDescriptor& descriptor : catalog.locales())
    {
        if (!valid_id(descriptor.id, BuiltinLocaleId::Count))
            return std::unexpected("Built-in locale id is invalid.");
        auto& destination =
            prepared.translations[builtin_resource_index(descriptor.id)];
        if (!destination.empty())
            return std::unexpected("Built-in locale id is duplicated.");

        const std::filesystem::path path = catalog.resolve(descriptor.relative_path);
        const auto document = elysia::io::load_strict_json(path);
        if (!document)
            return std::unexpected("Built-in i18n load failed: " + document.error().message);
        if (!document->is_object() || document->size() != 1 || !document->contains("engine"))
            return std::unexpected(make_prepare_error("Built-in i18n root is invalid", path));

        BuiltinTranslationTable table;
        if (!flatten_translation_json(*document, "", table))
            return std::unexpected(make_prepare_error("Built-in i18n structure is invalid", path));
        destination = std::move(table);
    }

    for (const BuiltinSoundDescriptor& descriptor : catalog.sounds())
    {
        if (!valid_id(descriptor.id, BuiltinSoundId::Count))
            return std::unexpected("Built-in sound id is invalid.");
        auto& destination = prepared.sounds[builtin_resource_index(descriptor.id)];
        if (destination)
            return std::unexpected("Built-in sound id is duplicated.");

        const std::filesystem::path path = catalog.resolve(descriptor.relative_path);
        BuiltinSoundPtr sound(MIX_LoadAudio(nullptr,path.string().c_str(),true));
        if (!sound)
            return std::unexpected(make_prepare_error("Built-in sound load failed", path));
        destination = std::move(sound);
    }

    for (const BuiltinMusicDescriptor& descriptor : catalog.music())
    {
        if (!valid_id(descriptor.id, BuiltinMusicId::Count))
            return std::unexpected("Built-in music id is invalid.");
        auto& destination = prepared.music[builtin_resource_index(descriptor.id)];
        if (destination)
            return std::unexpected("Built-in music id is duplicated.");

        const std::filesystem::path path = catalog.resolve(descriptor.relative_path);
        BuiltinMusicPtr music(MIX_LoadAudio(nullptr,path.string().c_str(),false));
        if (!music)
            return std::unexpected(make_prepare_error("Built-in music load failed", path));
        destination = std::move(music);
    }

    return prepared;
}
}
