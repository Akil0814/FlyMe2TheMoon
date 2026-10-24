#pragma once

#include "world_text.h"

namespace elysia::gameplay::ui
{
struct SpeechBubbleStyle
{
    elysia::core::Color background{24, 28, 36, 255};
    elysia::core::Color border{220, 225, 235, 255};
    float border_width = 1.0f;
    float padding = 6.0f;
    float tail_width = 8.0f;
    float tail_height = 6.0f;
};

class SpeechBubble
{
public:
    SpeechBubble();
    [[nodiscard]] WorldText& text() noexcept { return _text; }
    [[nodiscard]] const WorldText& text() const noexcept { return _text; }
    void set_style(SpeechBubbleStyle style) noexcept;
    void set_visible(bool visible) noexcept { _visible = visible; }
    [[nodiscard]] bool is_visible() const noexcept { return _visible; }
    [[nodiscard]] elysia::core::Rect body_rect(elysia::core::Vector2 tip) const;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands,
        elysia::core::Vector2 tip) const;

private:
    WorldText _text;
    SpeechBubbleStyle _style;
    bool _visible = true;
};
}
