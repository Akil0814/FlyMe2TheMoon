#include "ui_blink_label.h"

#include <utility>

namespace elysia::ui
{
UiBlinkLabel::UiBlinkLabel(const elysia::core::Rect& rect,int order,UiTextContent text_content) noexcept
    : UiLabel(rect,order,std::move(text_content)) {}

UiBlinkLabel::UiBlinkLabel( const elysia::core::Vector2& position,const elysia::core::Vector2& size,
    int order,UiTextContent text_content) noexcept : UiLabel(position,size,order,std::move(text_content)) {}

UiBlinkLabel::UiBlinkLabel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,int order,UiTextContent text_content) noexcept : UiLabel(center,size,from_center,order,std::move(text_content)) {}

void UiBlinkLabel::reset() noexcept
{
    UiLabel::reset();
    _blink.reset();
    _end_emitted = false;
    set_opacity(_blink.opacity());
}

void UiBlinkLabel::configure_playback(effects::UiOpacityBlinkMode mode,
    double hold_time,double visible_duration,double hidden_duration,std::optional<int> blink_cycles)
{
    _blink.configure_playback(mode,hold_time,visible_duration,hidden_duration,blink_cycles);
}

void UiBlinkLabel::play() noexcept
{
    _end_emitted = false;
    _blink.play();
    set_opacity(_blink.opacity());
    if (_blink.is_finished())
        notify_finished();
}

void UiBlinkLabel::update(double delta)
{
    const bool finished = _blink.update(delta);
    set_opacity(_blink.opacity());
    if (finished)
        notify_finished();
}

void UiBlinkLabel::set_on_end(BlinkLabelOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiBlinkLabel::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    const BlinkLabelOnEnd callback = _on_end;
    if (callback)
        callback();
}
}
