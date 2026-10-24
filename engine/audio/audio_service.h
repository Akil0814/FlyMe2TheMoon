#pragma once

#include "audio_settings.h"
#include "mixer_backend.h"
#include "music_playback_controller.h"
#include "sound_playback_scheduler.h"
#include "sound_playback_types.h"
#include "../tools/singleton.h"

#include <SDL3_mixer/SDL_mixer.h>
#include <array>
#include <string_view>


#define ELYSIA_AUDIO ::elysia::audio::AudioService::instance()

namespace elysia::audio
{
class AudioService : public elysia::tools::Singleton<AudioService>
{
    friend elysia::tools::Singleton<AudioService>;

public:
    bool initialize(const AudioSettings& settings);
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }

    bool play_sound(const std::string_view& key, int loops = 0);
    SoundRequestResult request_sound(const std::string_view& key, const SoundPlayOptions& options = {});
    void update(double delta_seconds);
    bool stop_sound(SoundHandle handle,std::chrono::milliseconds fade_out = {});
    void cancel_all_scheduled_sounds();

    bool set_sound_group_config(SoundGroup group,const SoundGroupConfig& config);
    const SoundGroupConfig& sound_group_config(SoundGroup group) const;
    void set_sound_group_volume(SoundGroup group,int volume);
    [[nodiscard]] int sound_group_volume(SoundGroup group) const;

    bool play_music(const std::string_view& key, int loops = -1,std::chrono::milliseconds fade_in = {});
    bool transition_music(const std::string_view& key,const MusicTransitionOptions& options = {});
    void stop_music(std::chrono::milliseconds fade_out = {});
    void stop_all_sounds(std::chrono::milliseconds fade_out = {});

    void set_master_volume(int volume);
    void set_music_volume(int volume);
    void set_sound_volume(int volume);

    const AudioSettings& settings() const;

private:
    bool channel_playing(int channel) const;
    void halt_channel(int channel);
    static bool play_track(MIX_Track* track,MIX_Audio* audio,int loops);
    int start_sound(const std::string_view& key, int loops, SoundGroup group,double gain);
    MusicPlaybackController::Backend music_backend();
    void apply_music_volume(double gain) const;
    void apply_volumes();
    void apply_sound_group_volume(SoundGroup group);
    void apply_sound_channel_volume(int channel,SoundGroup group,double gain) const;
    static int clamp_volume(int volume);

private:
    MIX_Track*& _music_track = detail::mixer_backend().music;
    std::array<MIX_Track*,kSoundChannelCount>& _tracks = detail::mixer_backend().tracks;
    AudioSettings _settings{};
    SoundPlaybackScheduler _sound_scheduler;
    MusicPlaybackController _music_controller;
    std::array<int,kSoundGroupCount> _sound_group_volumes{ 100,100,100,100 };
    bool _initialized = false;
};

}
