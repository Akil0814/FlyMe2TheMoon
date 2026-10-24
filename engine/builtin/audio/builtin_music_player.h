#pragma once

#include "../resources/builtin_resource_ids.h"
#include "../../audio/audio_settings.h"
#include "../../tools/singleton.h"

#include <chrono>

struct MIX_Track;

namespace elysia::builtin
{
class BuiltinMusicPlayerTestAccess;

// Application owns the SDL lifetime. instance() constructs an empty player.
class BuiltinMusicPlayer final : public elysia::tools::Singleton<BuiltinMusicPlayer>
{
    friend elysia::tools::Singleton<BuiltinMusicPlayer>;
    friend class BuiltinMusicPlayerTestAccess;

public:
    [[nodiscard]] bool initialize(const elysia::audio::AudioSettings& settings);
    void shutdown() noexcept;
    [[nodiscard]] bool is_initialized() const noexcept;
    [[nodiscard]] bool play(BuiltinMusicId id, int loops = -1,
        std::chrono::milliseconds fade_in = {});
    void stop() noexcept;
    void set_master_volume(int volume);
    void set_music_volume(int volume);

private:
    BuiltinMusicPlayer() = default;
    ~BuiltinMusicPlayer() = default;
    [[nodiscard]] bool apply_volume();

    MIX_Track* _track = nullptr;
    int _master_volume = 100;
    int _music_volume = 100;
};
}
