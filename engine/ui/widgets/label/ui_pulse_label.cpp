#include "ui_pulse_label.h"

#include <utility>

namespace elysia::ui
{
UiPulseLabel::UiPulseLabel(const elysia::core::Rect& rect,int order,UiTextContent text_content) noexcept
    : UiLabel(rect,order,std::move(text_content)) {}

UiPulseLabel::UiPulseLabel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,
    int order,UiTextContent text_content) noexcept : UiLabel(position,size,order,std::move(text_content)) {}

UiPulseLabel::UiPulseLabel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
    UiFromCenterTag,int order,UiTextContent text_content) noexcept : UiLabel(center,size,from_center,order,std::move(text_content)) {}

void UiPulseLabel::reset() noexcept
{
    UiLabel::reset();
    _pulse.reset();
    _end_emitted = false;
    set_opacity(_pulse.opacity());
}

void UiPulseLabel::configure_playback(effects::UiOpacityPulseMode mode,
    double hold_time,double pulse_in_duration,double pulse_out_duration,
    std::optional<int> pulse_cycles,std::uint8_t min_alpha,std::uint8_t max_alpha)
{
    _pulse.configure_playback(mode,hold_time,pulse_in_duration,pulse_out_duration,pulse_cycles,min_alpha,max_alpha);
}

void UiPulseLabel::play() noexcept
{
    _end_emitted = false;
    _pulse.play();
    set_opacity(_pulse.opacity());
    if (_pulse.is_finished())
        notify_finished();
}

void UiPulseLabel::update(double delta)
{
    const bool finished = _pulse.update(delta);
    set_opacity(_pulse.opacity());
    if (finished)
        notify_finished();
}

void UiPulseLabel::set_on_end(PulseLabelOnEnd on_end)
{
    _on_end = std::move(on_end);
}

void UiPulseLabel::notify_finished()
{
    if (_end_emitted)
        return;
    _end_emitted = true;
    const PulseLabelOnEnd callback = _on_end;
    if (callback)
        callback();
}
}
