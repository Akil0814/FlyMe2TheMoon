#include "music_playback_controller.h"

namespace elysia::audio
{
void MusicPlaybackController::reset()
{
    _key.clear();
    _pending.reset();
    _fade.reset();
    _stopping = false;
}

bool MusicPlaybackController::start(const Request& request,const Backend& backend)
{
    _fade.reset(request.fade_in.count() > 0 ? 0.0 : 1.0);
    _stopping = false;
    if (!backend.start(request.key,request.loops,_fade.gain()))
    {
        reset();
        return false;
    }
    _key = request.key;
    _fade.start(1.0,request.fade_in);
    return true;
}

bool MusicPlaybackController::start_pending(const Backend& backend)
{
    _key.clear();
    _stopping = false;
    if (!_pending) { _fade.reset(); return true; }
    const auto request = std::move(*_pending);
    _pending.reset();
    return start(request,backend);
}

bool MusicPlaybackController::play(std::string_view key,int loops,std::chrono::milliseconds fade_in,const Backend& backend)
{
    backend.stop();
    reset();
    return start({std::string(key),loops,fade_in},backend);
}

bool MusicPlaybackController::transition(std::string_view key,const MusicTransitionOptions& options,const Backend& backend)
{
    if (!backend.playing())
    {
        reset();
        return start({std::string(key),options.loops,options.fade_in},backend);
    }
    if (_key == key)
    {
        _pending.reset();
        _stopping = false;
        _fade.start(1.0,options.fade_in);
        backend.volume(_fade.gain());
        return true;
    }
    _pending = Request{std::string(key),options.loops,options.fade_in};
    if (!_stopping)
    {
        _stopping = true;
        _fade.start(0.0,options.fade_out);
    }
    if (_fade.finished())
    {
        backend.stop();
        return start_pending(backend);
    }
    return true;
}

void MusicPlaybackController::stop(std::chrono::milliseconds fade_out,const Backend& backend)
{
    _pending.reset();
    if (fade_out.count() <= 0 || !backend.playing())
    {
        backend.stop();
        reset();
    }
    else if (!_stopping)
    {
        _stopping = true;
        _fade.start(0.0,fade_out);
    }
}

void MusicPlaybackController::update(double seconds,const Backend& backend)
{
    // The built-in player shares SDL's music channel but is outside this controller.
    if (_key.empty() && !_pending && !_stopping) return;
    if (!backend.playing()) { (void)start_pending(backend); return; }
    _fade.update(seconds);
    backend.volume(_fade.gain());
    if (_stopping && _fade.finished())
    {
        backend.stop();
        (void)start_pending(backend);
    }
}
}
