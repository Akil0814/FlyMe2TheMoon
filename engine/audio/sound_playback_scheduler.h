#pragma once

#include "sound_playback_types.h"
#include "audio_fade.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::audio
{
class SoundPlaybackScheduler
{
public:
    // The final argument is the initial gain, applied before playback begins.
    using StartSoundCallback = std::function<int(std::string_view,int,SoundGroup,double)>;
    using ChannelPlayingCallback = std::function<bool(int)>;
    using StopSoundCallback = std::function<void(int)>;
    using ActiveChannelCallback = std::function<void(int,double)>;
    using VolumeCallback = std::function<void(int,SoundGroup,double)>;

    [[nodiscard]] bool set_group_config(SoundGroup group,const SoundGroupConfig& config);
    [[nodiscard]] const SoundGroupConfig& group_config(SoundGroup group) const;

    [[nodiscard]] SoundRequestResult request_sound(std::string_view key,const SoundPlayOptions& options,
        const StartSoundCallback& start_sound,const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound = {});

    void update(double delta_seconds,const StartSoundCallback& start_sound,
        const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound = {},const VolumeCallback& volume = {});

    [[nodiscard]] bool stop_sound(SoundHandle handle,const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound,std::chrono::milliseconds fade_out = {});
    void stop_all_sounds(const ChannelPlayingCallback& playing,const StopSoundCallback& stop,std::chrono::milliseconds fade_out = {});
    void cancel_all_scheduled_sounds();
    void clear_active_sounds();
    void for_each_active_channel(SoundGroup group,const ChannelPlayingCallback& is_channel_playing,const ActiveChannelCallback& callback);
    void reset();

private:
    struct ActiveSound
    {
        SoundHandle handle = 0;
        int channel = -1;
        SoundGroup group = SoundGroup::Extra;
        AudioFade fade;
        bool stopping = false;
    };

    struct PendingSound
    {
        SoundHandle handle = 0;
        std::string key;
        SoundPlayOptions options{};
        double due_time_seconds = 0.0;
    };

    [[nodiscard]] bool try_start_sound(SoundHandle handle,std::string_view key,
        const SoundPlayOptions& options,const StartSoundCallback& start_sound,
        const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound);
    void prune_finished_sounds(const ChannelPlayingCallback& is_channel_playing);
    [[nodiscard]] std::size_t active_count(SoundGroup group) const;

private:
    std::array<SoundGroupConfig,kSoundGroupCount> _group_configs{};
    std::vector<ActiveSound> _active_sounds;
    std::vector<PendingSound> _pending_sounds;
    std::unordered_map<std::string,double> _last_started_seconds_by_key;
    double _elapsed_seconds = 0.0;
    SoundHandle _next_sound_handle = 1;
};
}
