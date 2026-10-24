#include "ui_fade_label.h"

#include <utility>

namespace elysia::ui
{
UiFadeLabel::UiFadeLabel(const elysia::core::Rect& rect,int order,UiTextContent text_content) noexcept
    : UiLabel(rect,order,std::move(text_content)) {}

UiFadeLabel::UiFadeLabel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,
    int order,UiTextContent text_content) noexcept : UiLabel(position,size,order,std::move(text_content)) {}

UiFadeLabel::UiFadeLabel(
    const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,int order,UiTextContent text_content) noexcept : UiLabel(center,size,from_center,order,std::move(text_content)) {}

void UiFadeLabel::reset() noexcept
{
    UiLabel::reset();
    _fade.reset();
    _end_emitted = false;
    set_opacity(_fade.opacity());
}

void UiFadeLabel::configure_playback(effects::UiOpacityFadeMode mode,double hold_time,double fade_in_duration,double fade_out_duration)
{
    _fade.configure_playback(mode,hold_time,fade_in_duration,fade_out_duration);
}

void UiFadeLabel::play() noexcept
{
    _end_emitted = false;
    _fade.play();
    set_opacity(_fade.opacity());
    if (_fade.is_finished())
        notify_finished();
}

void UiFadeLabel::update(double delta)
{
    const bool finished = _fade.update(delta);
    set_opacity(_fade.opacity());
    if (finished)
        notify_finished();
}

void UiFadeLabel::set_on_end(FadeLabelOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiFadeLabel::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    destroy();
    const FadeLabelOnEnd callback = _on_end;
    if (callback)
        callback();
}
}
