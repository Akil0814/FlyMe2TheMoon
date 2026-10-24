#include "sound_playback_scheduler.h"

#include <algorithm>

namespace elysia::audio
{
bool SoundPlaybackScheduler::set_group_config(SoundGroup group,const SoundGroupConfig& config)
{
    if (config.max_simultaneous && *config.max_simultaneous > sound_group_hard_limit(group))
        return false;
    if (config.cooldown.count() < 0)
        return false;

    _group_configs[sound_group_index(group)] = config;
    return true;
}

const SoundGroupConfig& SoundPlaybackScheduler::group_config(SoundGroup group) const
{
    return _group_configs[sound_group_index(group)];
}

SoundRequestResult SoundPlaybackScheduler::request_sound(std::string_view key,
    const SoundPlayOptions& options,const StartSoundCallback& start_sound,
    const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound)
{
    if (key.empty())
        return {};

    const SoundHandle handle = _next_sound_handle++;
    if (options.start_delay.count() <= 0)
    {
        if (!try_start_sound(handle,key,options,start_sound,is_channel_playing,stop_sound))
            return {};

        return { SoundRequestStatus::Started,handle };
    }

    _pending_sounds.push_back({ handle,std::string(key),options,_elapsed_seconds + options.start_delay.count() / 1000.0 });
    return { SoundRequestStatus::Scheduled,handle };
}

void SoundPlaybackScheduler::update(double delta_seconds,
    const StartSoundCallback& start_sound,const ChannelPlayingCallback& is_channel_playing,
    const StopSoundCallback& stop_sound,const VolumeCallback& volume)
{
    delta_seconds = audio_delta(delta_seconds);
    _elapsed_seconds += delta_seconds;
    prune_finished_sounds(is_channel_playing);

    auto active = _active_sounds.begin();
    while (active != _active_sounds.end())
    {
        active->fade.update(delta_seconds);
        if (volume) volume(active->channel,active->group,active->fade.gain());
        if (active->stopping && active->fade.finished() && stop_sound)
        {
            stop_sound(active->channel);
            active = _active_sounds.erase(active);
        }
        else ++active;
    }

    auto pending = _pending_sounds.begin();
    while (pending != _pending_sounds.end())
    {
        if (pending->due_time_seconds > _elapsed_seconds)
        {
            ++pending;
            continue;
        }

        (void)try_start_sound(pending->handle,pending->key,pending->options,start_sound,is_channel_playing,stop_sound);
        pending = _pending_sounds.erase(pending);
    }
}

bool SoundPlaybackScheduler::stop_sound(SoundHandle handle,
    const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound,std::chrono::milliseconds fade_out)
{
    const auto pending = std::find_if(_pending_sounds.begin(),_pending_sounds.end(),
        [handle](const PendingSound& sound) { return sound.handle == handle; });
    if (pending != _pending_sounds.end())
    {
        _pending_sounds.erase(pending);
        return true;
    }

    prune_finished_sounds(is_channel_playing);
    const auto active = std::find_if(_active_sounds.begin(),_active_sounds.end(),
        [handle](const ActiveSound& sound) { return sound.handle == handle; });
    if (active == _active_sounds.end() || !stop_sound)
        return false;

    if (fade_out.count() <= 0)
    {
        stop_sound(active->channel);
        _active_sounds.erase(active);
    }
    else if (!active->stopping)
    {
        active->stopping = true;
        active->fade.start(0.0,fade_out);
    }

    return true;
}

void SoundPlaybackScheduler::stop_all_sounds(const ChannelPlayingCallback& playing,
    const StopSoundCallback& stop,std::chrono::milliseconds fade_out)
{
    prune_finished_sounds(playing);
    std::vector<SoundHandle> handles;
    for (const auto& sound : _active_sounds) handles.push_back(sound.handle);
    for (auto handle : handles) (void)stop_sound(handle,playing,stop,fade_out);
}

void SoundPlaybackScheduler::cancel_all_scheduled_sounds()
{
    _pending_sounds.clear();
}

void SoundPlaybackScheduler::clear_active_sounds()
{
    _active_sounds.clear();
}

void SoundPlaybackScheduler::for_each_active_channel(SoundGroup group,
    const ChannelPlayingCallback& is_channel_playing,const ActiveChannelCallback& callback)
{
    prune_finished_sounds(is_channel_playing);
    for (const ActiveSound& sound : _active_sounds)
        if (sound.group == group)
            callback(sound.channel,sound.fade.gain());
}

void SoundPlaybackScheduler::reset()
{
    _active_sounds.clear();
    _pending_sounds.clear();
    _last_started_seconds_by_key.clear();
    _elapsed_seconds = 0.0;
    _next_sound_handle = 1;
}

bool SoundPlaybackScheduler::try_start_sound(SoundHandle handle,std::string_view key,
    const SoundPlayOptions& options,const StartSoundCallback& start_sound,
    const ChannelPlayingCallback& is_channel_playing,const StopSoundCallback& stop_sound)
{
    prune_finished_sounds(is_channel_playing);

    const SoundGroupConfig& config = group_config(options.group);
    const auto last_started = _last_started_seconds_by_key.find(std::string(key));
    const double cooldown_seconds = config.cooldown.count() / 1000.0;
    if (last_started != _last_started_seconds_by_key.end()
        && _elapsed_seconds - last_started->second < cooldown_seconds)
        return false;

    const std::size_t group_limit = config.max_simultaneous.value_or(sound_group_hard_limit(options.group));
    if (active_count(options.group) >= group_limit)
    {
        if (config.overflow_policy == SoundOverflowPolicy::IgnoreNew || !stop_sound)
            return false;

        const auto oldest = std::find_if(_active_sounds.begin(),_active_sounds.end(),
            [&options](const ActiveSound& sound) { return sound.group == options.group; });
        if (oldest == _active_sounds.end())
            return false;

        stop_sound(oldest->channel);
        _active_sounds.erase(oldest);
    }

    if (_active_sounds.size() >= kSoundChannelCount)
        return false;

    AudioFade fade;
    fade.reset(options.fade_in.count() > 0 ? 0.0 : 1.0);
    fade.start(1.0,options.fade_in);
    const int channel = start_sound(key,options.loops.value_or(0),options.group,fade.gain());
    if (channel < 0)
        return false;

    _active_sounds.push_back({ handle,channel,options.group,fade,false });
    _last_started_seconds_by_key[std::string(key)] = _elapsed_seconds;
    return true;
}

void SoundPlaybackScheduler::prune_finished_sounds(const ChannelPlayingCallback& is_channel_playing)
{
    std::erase_if(_active_sounds,[&is_channel_playing](const ActiveSound& sound)
    {
        return !is_channel_playing(sound.channel);
    });
}

std::size_t SoundPlaybackScheduler::active_count(SoundGroup group) const
{
    return static_cast<std::size_t>(std::count_if(_active_sounds.begin(),_active_sounds.end(),
        [group](const ActiveSound& sound) { return sound.group == group; }));
}
}
