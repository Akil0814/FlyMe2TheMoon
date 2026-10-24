#pragma once
#include "sound_playback_types.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <array>

namespace elysia::audio::detail
{
// Shared by built-in startup audio and project audio. Resource caches own MIX_Audio.
class MixerBackend
{
public:
    bool initialize()
    {
        if (mixer) return true;
        mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,nullptr);
        if (!mixer) return false;
        music = MIX_CreateTrack(mixer);
        if (!music) { shutdown(); return false; }
        for (auto& track : tracks)
        {
            track = MIX_CreateTrack(mixer);
            if (!track) { shutdown(); return false; }
        }
        return true;
    }
    void shutdown()
    {
        if (mixer) MIX_DestroyMixer(mixer);
        mixer = nullptr; music = nullptr; tracks.fill(nullptr);
    }
    static void stop(MIX_Track* track)
    {
        if (track) { MIX_StopTrack(track,0); MIX_SetTrackAudio(track,nullptr); }
    }
    static bool play(MIX_Track* track,MIX_Audio* audio,int loops)
    {
        if (!track || !audio || !MIX_SetTrackAudio(track,audio)) return false;
        const auto options = SDL_CreateProperties();
        if (!options) { MIX_SetTrackAudio(track,nullptr); return false; }
        const bool result = SDL_SetNumberProperty(options,MIX_PROP_PLAY_LOOPS_NUMBER,loops) && MIX_PlayTrack(track,options);
        SDL_DestroyProperties(options);
        if (!result) MIX_SetTrackAudio(track,nullptr);
        return result;
    }
    void destroy_audio(MIX_Audio* audio)
    {
        if (!audio) return;
        if (music && MIX_GetTrackAudio(music) == audio) stop(music);
        for (auto* track : tracks)
            if (track && MIX_GetTrackAudio(track) == audio) stop(track);
        MIX_DestroyAudio(audio);
    }
    MIX_Mixer* mixer = nullptr;
    MIX_Track* music = nullptr;
    std::array<MIX_Track*,kSoundChannelCount> tracks{};
};
inline MixerBackend& mixer_backend() { static MixerBackend value; return value; }
}
