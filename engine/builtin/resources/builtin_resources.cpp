#include "builtin_resources.h"
#include "../audio/builtin_music_player.h"

namespace elysia::builtin
{
// The cache handles its own destruction. Do not access another singleton here:
// the player may already have been destroyed during process teardown.
BuiltinResources::~BuiltinResources() = default;

std::expected<void, std::string> BuiltinResources::initialize(
    SDL_Renderer* renderer,
    const BuiltinAssetCatalog& catalog,
    std::span<const int> point_sizes)
{
    BuiltinMusicPlayer::instance()->stop();
    return _assets.initialize(renderer, catalog, point_sizes);
}

void BuiltinResources::shutdown() noexcept
{
    BuiltinMusicPlayer::instance()->stop();
    _assets.shutdown();
}

bool BuiltinResources::is_initialized() const noexcept
{
    return _assets.is_initialized();
}

SDL_Texture* BuiltinResources::find_texture(BuiltinTextureId id) const noexcept
{
    return _assets.find_texture(id);
}

TTF_Font* BuiltinResources::find_font(BuiltinFontId id,int point_size) const noexcept
{
    return _assets.find_font(id, point_size);
}

const std::string* BuiltinResources::find_translation(
    BuiltinLocaleId locale,std::string_view key) const noexcept
{
    return _assets.find_translation(locale, key);
}

const BuiltinAnimationDefinition* BuiltinResources::find_animation(
    BuiltinAnimationId id) const noexcept
{
    return _assets.find_animation(id);
}

MIX_Audio* BuiltinResources::find_sound(BuiltinSoundId id) const noexcept
{
    return _assets.find_sound(id);
}

MIX_Audio* BuiltinResources::find_music(BuiltinMusicId id) const noexcept
{
    return _assets.find_music(id);
}

std::unique_ptr<elysia::animation::Animation> BuiltinResources::create_animation(
    BuiltinAnimationId id) const
{
    return _assets.create_animation(id);
}

std::size_t BuiltinResources::texture_count() const noexcept { return _assets.texture_count(); }
std::size_t BuiltinResources::font_count() const noexcept { return _assets.font_count(); }
std::size_t BuiltinResources::locale_count() const noexcept { return _assets.locale_count(); }
std::size_t BuiltinResources::animation_count() const noexcept { return _assets.animation_count(); }
std::size_t BuiltinResources::sound_count() const noexcept { return _assets.sound_count(); }
std::size_t BuiltinResources::music_count() const noexcept { return _assets.music_count(); }
}
