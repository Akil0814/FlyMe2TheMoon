#include "ui_fade_image.h"

namespace elysia::ui
{
UiFadeImage::UiFadeImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order)
    : UiImage(texture,pos,size,order) {}

UiFadeImage::UiFadeImage(SDL_Texture* texture,elysia::core::Rect rect,int order)
    : UiImage(texture,rect,order) {}

UiFadeImage::UiFadeImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order)
    : UiImage(texture,center,image_size,from_center,order) {}

UiFadeImage::UiFadeImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 source_size,elysia::core::Vector2 render_size,
    UiFromCenterTag,int order ) : UiImage(texture,center,source_size,render_size,from_center,order) {}

void UiFadeImage::reset() noexcept
{
    UiImage::reset();
    _fade.reset();
    _end_emitted = false;
    set_opacity(_fade.opacity());
}

void UiFadeImage::configure_playback(effects::UiOpacityFadeMode mode,double hold_time,double fade_in_duration,double fade_out_duration)
{
    _fade.configure_playback(mode,hold_time,fade_in_duration,fade_out_duration);
}

void UiFadeImage::play()
{
    _end_emitted = false;
    _fade.play();
    set_opacity(_fade.opacity());
    if (_fade.is_finished())
        notify_finished();
}

void UiFadeImage::update(double delta)
{
    const bool finished = _fade.update(delta);
    set_opacity(_fade.opacity());
    if (finished)
        notify_finished();
}

void UiFadeImage::set_on_end(FadeImageOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiFadeImage::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    destroy();
    const FadeImageOnEnd callback = _on_end;
    if (callback)
        callback();
}

}
