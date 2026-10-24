#include "../tools/logger.h"
#include "audio_service.h"

#include "../resources/resource_service.h"

#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
namespace elysia::audio
{
bool AudioService::initialize(const AudioSettings& settings)
{
    if (_initialized)
        shutdown();

    _music_controller.reset();
    _settings.master_volume = clamp_volume(settings.master_volume);
    _settings.music_volume = clamp_volume(settings.music_volume);
    _settings.sound_volume = clamp_volume(settings.sound_volume);

    if (!detail::mixer_backend().initialize())
        return false;

    _sound_scheduler.reset();
    _sound_group_volumes.fill(100);
    _initialized = true;
    apply_volumes();
    return true;
}

void AudioService::shutdown()
{
    if (!_initialized)
        return;

    stop_music();
    stop_all_sounds();
    _sound_scheduler.reset();
    _initialized = false;
}

bool AudioService::play_sound(const std::string_view& key, int loops)
{
    SoundPlayOptions options{};
    options.loops = loops;
    return request_sound(key,options).status == SoundRequestStatus::Started;
}

SoundRequestResult AudioService::request_sound(const std::string_view& key, const SoundPlayOptions& options)
{
    if (!_initialized)
    {
        ELYSIA_LOG_WARN("audio","Play sound failed: audio service is not initialized.");
        return {};
    }

    MIX_Audio* sound = ELYSIA_RESOURCES->find_sound(key);
    if (!sound)
    {
        ELYSIA_LOG_WARN("audio","Play sound failed: sound does not exist: " << key);
        return {};
    }

    return _sound_scheduler.request_sound(key,options,
        [this](std::string_view scheduled_key,int scheduled_loops,SoundGroup group,double gain)
        {
            return start_sound(scheduled_key,scheduled_loops,group,gain);
        },
        [this](int channel)
        {
            return channel_playing(channel);
        },
        [this](int channel)
        {
            halt_channel(channel);
        });
}

void AudioService::update(double delta_seconds)
{
    if (!_initialized)
        return;

    _sound_scheduler.update(delta_seconds,
        [this](std::string_view scheduled_key,int scheduled_loops,SoundGroup group,double gain)
        {
            return start_sound(scheduled_key,scheduled_loops,group,gain);
        },
        [this](int channel)
        {
            return channel_playing(channel);
        },
        [this](int channel)
        {
            halt_channel(channel);
        },
        [this](int channel,SoundGroup group,double gain)
        {
            apply_sound_channel_volume(channel,group,gain);
        });
    _music_controller.update(delta_seconds,music_backend());
}

bool AudioService::stop_sound(SoundHandle handle,std::chrono::milliseconds fade_out)
{
    if (!_initialized)
        return false;

    return _sound_scheduler.stop_sound(handle,
        [this](int channel)
        {
            return channel_playing(channel);
        },
        [this](int channel)
        {
            halt_channel(channel);
        },fade_out);
}

void AudioService::cancel_all_scheduled_sounds()
{
    _sound_scheduler.cancel_all_scheduled_sounds();
}

bool AudioService::set_sound_group_config(SoundGroup group,const SoundGroupConfig& config)
{
    if (!_sound_scheduler.set_group_config(group,config))
    {
        ELYSIA_LOG_WARN("audio","Sound group config exceeds its fixed maximum.");
        return false;
    }

    return true;
}

const SoundGroupConfig& AudioService::sound_group_config(SoundGroup group) const
{
    return _sound_scheduler.group_config(group);
}

void AudioService::set_sound_group_volume(SoundGroup group,int volume)
{
    _sound_group_volumes[sound_group_index(group)] = clamp_volume(volume);
    apply_sound_group_volume(group);
}

int AudioService::sound_group_volume(SoundGroup group) const
{
    return _sound_group_volumes[sound_group_index(group)];
}

bool AudioService::play_music(const std::string_view& key,int loops,std::chrono::milliseconds fade_in)
{
    if (!_initialized || !ELYSIA_RESOURCES->find_music(key))
    {
        ELYSIA_LOG_WARN("audio","Play music rejected: service uninitialized or missing music: " << key);
        return false;
    }
    return _music_controller.play(key,loops,fade_in,music_backend());
}

bool AudioService::transition_music(const std::string_view& key,const MusicTransitionOptions& options)
{
    if (!_initialized || !ELYSIA_RESOURCES->find_music(key))
    {
        ELYSIA_LOG_WARN("audio","Music transition rejected: service uninitialized or missing music: " << key);
        return false;
    }
    return _music_controller.transition(key,options,music_backend());
}

void AudioService::stop_music(std::chrono::milliseconds fade_out)
{
    if (_initialized) _music_controller.stop(fade_out,music_backend());
}

void AudioService::stop_all_sounds(std::chrono::milliseconds fade_out)
{
    if (!_initialized) return;
    if (fade_out.count() <= 0)
    {
        halt_channel(-1);
        _sound_scheduler.clear_active_sounds();
    }
    else
        _sound_scheduler.stop_all_sounds(
            [this](int channel) { return channel_playing(channel); },
            [this](int channel) { halt_channel(channel); },fade_out);
}

MusicPlaybackController::Backend AudioService::music_backend()
{
    return {
        [this](std::string_view key,int loops,double gain)
        {
            MIX_Audio* music = ELYSIA_RESOURCES->find_music(key);
            if (!music)
            {
                ELYSIA_LOG_WARN("audio","Music no longer exists: " << key);
                return false;
            }
            apply_music_volume(gain);
            if (!play_track(_music_track,music,loops))
            {
                ELYSIA_LOG_WARN("audio","Play music failed: " << key << " error: " << SDL_GetError());
                return false;
            }
            return true;
        },
        [this] { if (_music_track) { MIX_StopTrack(_music_track,0); MIX_SetTrackAudio(_music_track,nullptr); } },
        [this] { return _music_track && MIX_TrackPlaying(_music_track); },
        [this](double gain) { apply_music_volume(gain); }
    };
}

void AudioService::apply_music_volume(double gain) const
{
    const double volume = _settings.master_volume * _settings.music_volume / 10000.0;
    MIX_SetTrackGain(_music_track,static_cast<float>(volume * gain));
}

void AudioService::set_master_volume(int volume)
{
    _settings.master_volume = clamp_volume(volume);
    apply_volumes();
}

void AudioService::set_music_volume(int volume)
{
    _settings.music_volume = clamp_volume(volume);
    apply_volumes();
}

void AudioService::set_sound_volume(int volume)
{
    _settings.sound_volume = clamp_volume(volume);
    apply_volumes();
}

const AudioSettings& AudioService::settings() const
{
    return _settings;
}

int AudioService::start_sound(const std::string_view& key, int loops, SoundGroup group,double gain)
{
    MIX_Audio* sound = ELYSIA_RESOURCES->find_sound(key);
    if (!sound)
    {
        ELYSIA_LOG_WARN("audio","Play sound failed: sound does not exist: " << key);
        return -1;
    }

    int channel = 0;
    while (channel < static_cast<int>(kSoundChannelCount) && channel_playing(channel)) ++channel;
    if (channel == static_cast<int>(kSoundChannelCount)) return -1;
    apply_sound_channel_volume(channel,group,gain);
    if (!play_track(_tracks[channel],sound,loops)) channel = -1;
    if (channel < 0)
        ELYSIA_LOG_WARN("audio","Play sound failed: " << key<< " error: " << SDL_GetError());


    return channel;
}

void AudioService::apply_volumes()
{
    if (!_initialized)
        return;

    apply_music_volume(_music_controller.gain());
    for (std::size_t index = 0; index < kSoundGroupCount; ++index)
        apply_sound_group_volume(static_cast<SoundGroup>(index));
}

void AudioService::apply_sound_group_volume(SoundGroup group)
{
    if (!_initialized)
        return;

    _sound_scheduler.for_each_active_channel(group,
        [this](int channel)
        {
            return channel_playing(channel);
        },
        [this,group](int channel,double gain)
        {
            apply_sound_channel_volume(channel,group,gain);
        });
}

void AudioService::apply_sound_channel_volume(int channel,SoundGroup group,double gain) const
{
    const double effective_sound = (_settings.master_volume* _settings.sound_volume
        * _sound_group_volumes[sound_group_index(group)]) / 1000000.0;
    MIX_SetTrackGain(_tracks[channel],static_cast<float>(effective_sound * gain));
}

int AudioService::clamp_volume(int volume)
{
    return std::clamp(volume, 0, 100);
}

}

namespace elysia::audio
{
bool AudioService::channel_playing(int channel) const
{
    return channel >= 0 && channel < static_cast<int>(_tracks.size()) && _tracks[channel] && MIX_TrackPlaying(_tracks[channel]);
}
void AudioService::halt_channel(int channel)
{
    if (channel == -1) { for (int i=0;i<static_cast<int>(_tracks.size());++i) halt_channel(i); return; }
    if (channel >= 0 && channel < static_cast<int>(_tracks.size()) && _tracks[channel])
    {
        MIX_StopTrack(_tracks[channel],0);
        MIX_SetTrackAudio(_tracks[channel],nullptr);
    }
}
bool AudioService::play_track(MIX_Track* track,MIX_Audio* audio,int loops)
{
    return detail::MixerBackend::play(track,audio,loops);
}
}
