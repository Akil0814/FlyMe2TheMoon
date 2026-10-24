#pragma once

#include <functional>

#include "ui_image.h"
#include "../../../core/interface/updatable.h"
#include "../../effects/ui_opacity_fade_core.h"

struct SDL_Texture;

namespace elysia::ui
{
class UiFadeImage : public UiImage, public elysia::core::Updatable
{
public:
    using FadeImageOnEnd = std::function<void()>;

    UiFadeImage(SDL_Texture* texture,elysia::core::Vector2 pos,elysia::core::Vector2 size,int order = 0);
    UiFadeImage(SDL_Texture* texture,elysia::core::Rect rect,int order = 0);
    UiFadeImage(SDL_Texture* texture,elysia::core::Vector2 center,elysia::core::Vector2 image_size,UiFromCenterTag,int order = 0);
    UiFadeImage(SDL_Texture* texture, elysia::core::Vector2 center, elysia::core::Vector2 source_size, elysia::core::Vector2 render_size,
        UiFromCenterTag, int order = 0);

    void reset() noexcept override;
    // Configures the fade sequence that modulates image opacity over time.
    void configure_playback(effects::UiOpacityFadeMode mode,double hold_time,double fade_in_duration,double fade_out_duration);
    void play();
    void update(double delta) override;
    void set_on_end(FadeImageOnEnd on_end);

private:
    // Emits the completion callback once per finished playback sequence.
    void notify_finished();

private:
    elysia::ui::effects::UiOpacityFadeCore _fade;
    FadeImageOnEnd _on_end;
    bool _end_emitted = false;
};
}
