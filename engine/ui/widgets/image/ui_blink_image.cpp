#include "ui_blink_image.h"

namespace elysia::ui
{
UiBlinkImage::UiBlinkImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order)
    : UiImage(texture,pos,size,order) {}

UiBlinkImage::UiBlinkImage(SDL_Texture* texture,elysia::core::Rect rect,int order)
    : UiImage(texture,rect,order) {}

UiBlinkImage::UiBlinkImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order)
    : UiImage(texture,center,image_size,from_center,order) {}

UiBlinkImage::UiBlinkImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 source_size,elysia::core::Vector2 render_size,
    UiFromCenterTag,int order ) : UiImage(texture,center,source_size,render_size,from_center,order) {}

void UiBlinkImage::reset() noexcept
{
    UiImage::reset();
    _blink.reset();
    _end_emitted = false;
    set_opacity(_blink.opacity());
}

void UiBlinkImage::configure_playback( effects::UiOpacityBlinkMode mode,double hold_time,
    double visible_duration,double hidden_duration,std::optional<int> blink_cycles)
{
    _blink.configure_playback(mode,hold_time,visible_duration,hidden_duration,blink_cycles);
}

void UiBlinkImage::play()
{
    _end_emitted = false;
    _blink.play();
    set_opacity(_blink.opacity());
    if (_blink.is_finished())
        notify_finished();
}

void UiBlinkImage::update(double delta)
{
    const bool finished = _blink.update(delta);
    set_opacity(_blink.opacity());
    if (finished)
        notify_finished();
}

void UiBlinkImage::set_on_end(BlinkImageOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiBlinkImage::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    const BlinkImageOnEnd callback = _on_end;
    if (callback)
        callback();
}
}
