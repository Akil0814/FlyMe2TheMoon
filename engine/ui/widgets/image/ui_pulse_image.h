#pragma once

#include <optional>
#include <cstdint>
#include <functional>

#include "ui_image.h"
#include "../../../core/interface/updatable.h"
#include "../../effects/ui_opacity_pulse_core.h"

struct SDL_Texture;

namespace elysia::ui
{
class UiPulseImage : public UiImage, public elysia::core::Updatable
{
public:
    using PulseImageOnEnd = std::function<void()>;

    UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order = 0);
    UiPulseImage(SDL_Texture* texture,elysia::core::Rect rect,int order = 0);
    UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order = 0);
    UiPulseImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 source_size, elysia::core::Vector2 render_size,
        UiFromCenterTag,int order = 0);

    void reset() noexcept override;
    // Configures the pulse sequence that modulates image opacity between min and max.
    void configure_playback(effects::UiOpacityPulseMode mode,
        double hold_time,double pulse_in_duration,double pulse_out_duration,
        std::optional<int> pulse_cycles = std::nullopt,std::uint8_t min_alpha = 96,std::uint8_t max_alpha = 255);
    void play();
    void update(double delta) override;
    void set_on_end(PulseImageOnEnd on_end);

private:
    // Emits the completion callback once per finished playback sequence.
    void notify_finished();

private:
    elysia::ui::effects::UiOpacityPulseCore _pulse;
    PulseImageOnEnd _on_end;
    bool _end_emitted = false;
};
}
