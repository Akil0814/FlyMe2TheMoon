#include "builtin_music_player.h"

#include "../resources/builtin_resources.h"
#include "../../audio/mixer_backend.h"
#include "../../tools/logger.h"

#include <algorithm>

namespace elysia::builtin
{
bool BuiltinMusicPlayer::initialize(const elysia::audio::AudioSettings& settings)
{
    _master_volume = std::clamp(settings.master_volume, 0, 100);
    _music_volume = std::clamp(settings.music_volume, 0, 100);
    if (!_track)
    {
        auto& backend = elysia::audio::detail::mixer_backend();
        if (!backend.initialize())
            return false;
        _track = MIX_CreateTrack(backend.mixer);
        if (!_track)
            return false;
    }
    if (!apply_volume())
    {
        shutdown();
        return false;
    }
    return true;
}

void BuiltinMusicPlayer::shutdown() noexcept
{
    stop();
    if (_track)
        MIX_DestroyTrack(_track);
    _track = nullptr;
}

bool BuiltinMusicPlayer::is_initialized() const noexcept
{
    return _track != nullptr;
}

bool BuiltinMusicPlayer::play(BuiltinMusicId id, int loops,
    std::chrono::milliseconds fade_in)
{
    if (!_track)
        return false;
    MIX_Audio* music = BuiltinResources::instance()->find_music(id);
    if (!music)
        return false;

    stop();
    const auto options = SDL_CreateProperties();
    const bool played = options
        && SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loops)
        && SDL_SetNumberProperty(options, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER,
            std::max<std::chrono::milliseconds::rep>(fade_in.count(), 0))
        && SDL_SetFloatProperty(options, MIX_PROP_PLAY_FADE_IN_START_GAIN_FLOAT, 0.0f)
        && apply_volume()
        && MIX_SetTrackAudio(_track, music)
        && MIX_PlayTrack(_track, options);
    if (options)
        SDL_DestroyProperties(options);
    if (!played)
    {
        ELYSIA_LOG_WARN("builtin", "Play music failed: " << builtin_resource_name(id)
            << " error: " << SDL_GetError());
        stop();
    }
    return played;
}

void BuiltinMusicPlayer::stop() noexcept
{
    elysia::audio::detail::MixerBackend::stop(_track);
}

bool BuiltinMusicPlayer::apply_volume()
{
    return !_track || MIX_SetTrackGain(_track,
        (_master_volume * _music_volume) / 10000.0f);
}

void BuiltinMusicPlayer::set_master_volume(int volume)
{
    _master_volume = std::clamp(volume, 0, 100);
    if (!apply_volume())
        ELYSIA_LOG_WARN("builtin", "Set music gain failed: " << SDL_GetError());
}

void BuiltinMusicPlayer::set_music_volume(int volume)
{
    _music_volume = std::clamp(volume, 0, 100);
    if (!apply_volume())
        ELYSIA_LOG_WARN("builtin", "Set music gain failed: " << SDL_GetError());
}
}
