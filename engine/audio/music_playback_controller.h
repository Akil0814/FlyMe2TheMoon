#pragma once

#include "audio_fade.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace elysia::audio
{
struct MusicTransitionOptions
{
    int loops = -1;
    std::chrono::milliseconds fade_out{0};
    std::chrono::milliseconds fade_in{0};
};

class MusicPlaybackController
{
public:
    struct Backend
    {
        // Apply the initial gain before starting playback, not after.
        std::function<bool(std::string_view,int,double)> start;
        std::function<void()> stop;
        std::function<bool()> playing;
        std::function<void(double)> volume;
    };
    bool play(std::string_view key,int loops,std::chrono::milliseconds fade_in,const Backend& backend);
    bool transition(std::string_view key,const MusicTransitionOptions& options,const Backend& backend);
    void stop(std::chrono::milliseconds fade_out,const Backend& backend);
    void update(double seconds,const Backend& backend);
    void reset();
    [[nodiscard]] double gain() const { return _fade.gain(); }
private:
    struct Request { std::string key; int loops; std::chrono::milliseconds fade_in; };
    bool start(const Request& request,const Backend& backend);
    bool start_pending(const Backend& backend);
    std::string _key;
    std::optional<Request> _pending;
    AudioFade _fade;
    bool _stopping = false;
};
}
