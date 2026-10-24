#include "ui_pulse_image.h"

namespace elysia::ui
{
UiPulseImage::UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order)
    : UiImage(texture,pos,size,order) {}

UiPulseImage::UiPulseImage(SDL_Texture* texture,elysia::core::Rect rect,int order)
    : UiImage(texture,rect,order) {}

UiPulseImage::UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order)
    : UiImage(texture,center,image_size,from_center,order) {}

UiPulseImage::UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 source_size,elysia::core::Vector2 render_size,
    UiFromCenterTag,int order ) : UiImage(texture,center,source_size,render_size,from_center,order) {}

void UiPulseImage::reset() noexcept
{
    UiImage::reset();
    _pulse.reset();
    _end_emitted = false;
    set_opacity(_pulse.opacity());
}

void UiPulseImage::configure_playback(effects::UiOpacityPulseMode mode,
    double hold_time,double pulse_in_duration,double pulse_out_duration,
    std::optional<int> pulse_cycles,std::uint8_t min_alpha, std::uint8_t max_alpha)
{
    _pulse.configure_playback(mode,hold_time,pulse_in_duration,pulse_out_duration,pulse_cycles,min_alpha,max_alpha);
}

void UiPulseImage::play()
{
    _end_emitted = false;
    _pulse.play();
    set_opacity(_pulse.opacity());
    if (_pulse.is_finished())
        notify_finished();
}

void UiPulseImage::update(double delta)
{
    const bool finished = _pulse.update(delta);
    set_opacity(_pulse.opacity());
    if (finished)
        notify_finished();
}

void UiPulseImage::set_on_end(PulseImageOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiPulseImage::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    const PulseImageOnEnd callback = _on_end;
    if (callback)
        callback();
}
}
