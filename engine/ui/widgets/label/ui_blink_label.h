#pragma once

#include <optional>
#include <functional>

#include "ui_label.h"
#include "../../../core/interface/updatable.h"
#include "../../effects/ui_opacity_blink_core.h"

namespace elysia::ui
{
class UiBlinkLabel : public UiLabel, public elysia::core::Updatable
{
public:
    using BlinkLabelOnEnd = std::function<void()>;

    explicit UiBlinkLabel(const elysia::core::Rect& rect = elysia::core::Rect::zero(),int order = 0,UiTextContent text_content = {}) noexcept;
    UiBlinkLabel(const elysia::core::Vector2& position, const elysia::core::Vector2& size,
        int order = 0, UiTextContent text_content = {}) noexcept;
    UiBlinkLabel(const elysia::core::Vector2& center,const elysia::core::Vector2& size,
        UiFromCenterTag,int order = 0, UiTextContent text_content = {}) noexcept;

    void reset() noexcept override;
    // Configures the blinking sequence that modulates label opacity over time.
    void configure_playback(effects::UiOpacityBlinkMode mode,
        double hold_time,double visible_duration,double hidden_duration,
        std::optional<int> blink_cycles = std::nullopt);
    void play() noexcept;
    void update(double delta) override;
    void set_on_end(BlinkLabelOnEnd on_end);

private:
    // Emits the completion callback once per finished playback sequence.
    void notify_finished();

private:
    elysia::ui::effects::UiOpacityBlinkCore _blink;
    BlinkLabelOnEnd _on_end;
    bool _end_emitted = false;
};
}
