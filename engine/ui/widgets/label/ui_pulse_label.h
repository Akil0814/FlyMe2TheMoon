#pragma once

#include <optional>
#include <cstdint>
#include <functional>

#include "ui_label.h"
#include "../../../core/interface/updatable.h"
#include "../../effects/ui_opacity_pulse_core.h"

namespace elysia::ui
{
class UiPulseLabel : public UiLabel, public elysia::core::Updatable
{
public:
    using PulseLabelOnEnd = std::function<void()>;

    explicit UiPulseLabel(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0,UiTextContent text_content = {}) noexcept;
    UiPulseLabel(const elysia::core::Vector2& position,const elysia::core::Vector2& size,
        int order = 0,UiTextContent text_content = {}) noexcept;
    UiPulseLabel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
        UiFromCenterTag,int order = 0,UiTextContent text_content = {}) noexcept;

    void reset() noexcept override;

    // Configures the pulse sequence that modulates label opacity between min and max.
    void configure_playback(effects::UiOpacityPulseMode mode,
        double hold_time,double pulse_in_duration,double pulse_out_duration,
        std::optional<int> pulse_cycles = std::nullopt,std::uint8_t min_alpha = 96,std::uint8_t max_alpha = 255);

    void play() noexcept;
    void update(double delta) override;
    void set_on_end(PulseLabelOnEnd on_end);

private:
    // Emits the completion callback once per finished playback sequence.
    void notify_finished();

private:
    elysia::ui::effects::UiOpacityPulseCore _pulse;
    PulseLabelOnEnd _on_end;
    bool _end_emitted = false;
};
}
